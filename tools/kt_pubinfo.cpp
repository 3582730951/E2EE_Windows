#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "crypto.h"
#include "hex_utils.h"
#include "key_transparency.h"
#include "qrcodegen.hpp"

namespace {

struct Options {
  std::filesystem::path input{"kt_root_pub.bin"};
  std::filesystem::path qr_svg;
  bool show_help{false};
};

void PrintUsage() {
  std::cout
      << "Usage: mi_e2ee_kt_pubinfo [--in PATH] [--qr-svg PATH]\n"
         "  --in PATH       Path to kt_root_pub.bin (default: ./kt_root_pub.bin)\n"
         "  --qr-svg PATH   Write a QR code SVG with the fingerprint payload\n";
}

bool ParseArgs(int argc, char** argv, Options& out, std::string& error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      out.show_help = true;
      return true;
    }
    if (arg == "--in") {
      if (i + 1 >= argc) {
        error = "--in requires a value";
        return false;
      }
      out.input = argv[++i];
      continue;
    }
    if (arg == "--qr-svg") {
      if (i + 1 >= argc) {
        error = "--qr-svg requires a value";
        return false;
      }
      out.qr_svg = argv[++i];
      continue;
    }
    error = "unknown argument: " + arg;
    return false;
  }
  return true;
}

bool ReadFileBytes(const std::filesystem::path& path,
                   std::vector<std::uint8_t>& out,
                   std::string& error) {
  error.clear();
  out.clear();
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "kt root pubkey not found";
    return false;
  }
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "kt root pubkey read failed";
    return false;
  }
  if (size == 0) {
    error = "kt root pubkey empty";
    return false;
  }
  if (size > static_cast<std::uintmax_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "kt root pubkey too large";
    return false;
  }
  out.resize(static_cast<std::size_t>(size));
  ifs.read(reinterpret_cast<char*>(out.data()),
           static_cast<std::streamsize>(out.size()));
  if (!ifs || ifs.gcount() != static_cast<std::streamsize>(out.size())) {
    error = "kt root pubkey read failed";
    return false;
  }
  return true;
}

std::string FingerprintSasHex(const std::string& sha256_hex) {
  std::vector<std::uint8_t> fp_bytes;
  if (!mi::common::HexToBytes(sha256_hex, fp_bytes) ||
      fp_bytes.size() != 32) {
    return {};
  }
  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_KT_ROOT_SAS_V1";
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  msg.insert(msg.end(), fp_bytes.begin(), fp_bytes.end());
  const std::string digest =
      mi::common::Sha256Hex(msg.data(), msg.size());
  if (digest.size() < 20) {
    return {};
  }
  return mi::common::GroupHex4(digest.substr(0, 20));
}

bool WriteQrSvg(const qrcodegen::QrCode& qr,
                const std::filesystem::path& out_path,
                std::string& error) {
  error.clear();
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "write qr svg failed";
    return false;
  }
  const int border = 4;
  const int scale = 6;
  const int size = qr.getSize();
  const int total = (size + border * 2) * scale;
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\"";
  out << " width=\"" << total << "\" height=\"" << total << "\"";
  out << " viewBox=\"0 0 " << total << " " << total << "\">\n";
  out << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
  out << "<g fill=\"#000000\">\n";
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (!qr.getModule(x, y)) {
        continue;
      }
      const int rx = (x + border) * scale;
      const int ry = (y + border) * scale;
      out << "<rect x=\"" << rx << "\" y=\"" << ry
          << "\" width=\"" << scale << "\" height=\"" << scale << "\"/>\n";
    }
  }
  out << "</g>\n</svg>\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  std::string error;
  if (!ParseArgs(argc, argv, opt, error)) {
    std::cerr << "[mi_e2ee_kt_pubinfo] " << error << "\n";
    PrintUsage();
    return 1;
  }
  if (opt.show_help) {
    PrintUsage();
    return 0;
  }

  std::vector<std::uint8_t> key_bytes;
  if (!ReadFileBytes(opt.input, key_bytes, error)) {
    std::cerr << "[mi_e2ee_kt_pubinfo] " << error << "\n";
    return 1;
  }
  if (key_bytes.size() != mi::server::kKtSthSigPublicKeyBytes) {
    std::cerr << "[mi_e2ee_kt_pubinfo] kt root pubkey size invalid\n";
    return 1;
  }

  const std::string fingerprint =
      mi::common::Sha256Hex(key_bytes.data(), key_bytes.size());
  const std::string sas = FingerprintSasHex(fingerprint);
  const std::string payload = "mi_e2ee_kt_root_sha256=" + fingerprint;

  std::cout << "kt_root_pub_sha256=" << fingerprint << "\n";
  if (!sas.empty()) {
    std::cout << "kt_root_pub_sas=" << sas << "\n";
  }
  std::cout << "qr_payload=" << payload << "\n";

  if (!opt.qr_svg.empty()) {
    const auto qr = qrcodegen::QrCode::encodeText(
        payload.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    if (!WriteQrSvg(qr, opt.qr_svg, error)) {
      std::cerr << "[mi_e2ee_kt_pubinfo] " << error << "\n";
      return 1;
    }
    std::cout << "qr_svg=" << opt.qr_svg.string() << "\n";
  }
  return 0;
}
