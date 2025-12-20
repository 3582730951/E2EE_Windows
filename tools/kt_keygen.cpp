#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "key_transparency.h"

extern "C" {
int PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
}

namespace {

struct Options {
  std::filesystem::path out_dir;
  std::filesystem::path signing_key;
  std::filesystem::path root_pub;
  bool force{false};
  bool show_help{false};
};

void PrintUsage() {
  std::cout
      << "Usage: mi_e2ee_kt_keygen [--out-dir DIR] [--sk PATH] [--pk PATH] "
         "[--force]\n"
         "  --out-dir DIR   Output directory (default: current directory)\n"
         "  --sk PATH       Output signing key path (default: kt_signing_key.bin)\n"
         "  --pk PATH       Output public key path (default: kt_root_pub.bin)\n"
         "  --force         Overwrite existing files\n";
}

bool ParseArgs(int argc, char** argv, Options& out, std::string& error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      out.show_help = true;
      return true;
    }
    if (arg == "--force") {
      out.force = true;
      continue;
    }
    if (arg == "--out-dir") {
      if (i + 1 >= argc) {
        error = "--out-dir requires a value";
        return false;
      }
      out.out_dir = argv[++i];
      continue;
    }
    if (arg == "--sk") {
      if (i + 1 >= argc) {
        error = "--sk requires a value";
        return false;
      }
      out.signing_key = argv[++i];
      continue;
    }
    if (arg == "--pk") {
      if (i + 1 >= argc) {
        error = "--pk requires a value";
        return false;
      }
      out.root_pub = argv[++i];
      continue;
    }
    error = "unknown argument: " + arg;
    return false;
  }
  return true;
}

void ResolvePaths(Options& opt) {
  if (opt.signing_key.empty()) {
    if (!opt.out_dir.empty()) {
      opt.signing_key = opt.out_dir / "kt_signing_key.bin";
    } else {
      opt.signing_key = "kt_signing_key.bin";
    }
  }
  if (opt.root_pub.empty()) {
    if (!opt.out_dir.empty()) {
      opt.root_pub = opt.out_dir / "kt_root_pub.bin";
    } else if (opt.signing_key.has_parent_path() &&
               !opt.signing_key.parent_path().empty()) {
      opt.root_pub = opt.signing_key.parent_path() / "kt_root_pub.bin";
    } else {
      opt.root_pub = "kt_root_pub.bin";
    }
  }
}

bool WriteFileAtomic(const std::filesystem::path& path,
                     const std::uint8_t* data,
                     std::size_t len,
                     bool overwrite,
                     std::string& error) {
  error.clear();
  if (path.empty() || !data || len == 0) {
    error = "key path empty";
    return false;
  }
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      error = "create output dir failed";
      return false;
    }
  }
  if (!overwrite && std::filesystem::exists(path, ec)) {
    if (ec) {
      error = "key path error";
      return false;
    }
    error = "output exists (use --force)";
    return false;
  }
  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      error = "write failed";
      return false;
    }
    ofs.write(reinterpret_cast<const char*>(data),
              static_cast<std::streamsize>(len));
    if (!ofs) {
      error = "write failed";
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  if (overwrite) {
    std::filesystem::remove(path, ec);
    if (ec) {
      std::filesystem::remove(tmp, ec);
      error = "remove failed";
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    error = "rename failed";
    return false;
  }
  return true;
}

bool GenerateKeyPair(const std::filesystem::path& signing_key,
                     const std::filesystem::path& root_pub,
                     bool overwrite,
                     std::string& error) {
  if (signing_key == root_pub) {
    error = "signing key and public key paths must differ";
    return false;
  }
  std::array<std::uint8_t, mi::server::kKtSthSigPublicKeyBytes> pk{};
  std::array<std::uint8_t, mi::server::kKtSthSigSecretKeyBytes> sk{};
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(pk.data(), sk.data()) != 0) {
    error = "keypair generation failed";
    return false;
  }
  if (!WriteFileAtomic(signing_key, sk.data(), sk.size(), overwrite, error)) {
    std::fill(sk.begin(), sk.end(), 0);
    return false;
  }
  if (!WriteFileAtomic(root_pub, pk.data(), pk.size(), overwrite, error)) {
    std::error_code ec;
    std::filesystem::remove(signing_key, ec);
    std::fill(sk.begin(), sk.end(), 0);
    return false;
  }
  std::fill(sk.begin(), sk.end(), 0);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  std::string error;
  if (!ParseArgs(argc, argv, opt, error)) {
    std::cerr << "[mi_e2ee_kt_keygen] " << error << "\n";
    PrintUsage();
    return 1;
  }
  if (opt.show_help) {
    PrintUsage();
    return 0;
  }
  ResolvePaths(opt);
  if (!GenerateKeyPair(opt.signing_key, opt.root_pub, opt.force, error)) {
    std::cerr << "[mi_e2ee_kt_keygen] " << error << "\n";
    return 1;
  }
  std::cout << "[mi_e2ee_kt_keygen] wrote " << opt.signing_key.string() << "\n";
  std::cout << "[mi_e2ee_kt_keygen] wrote " << opt.root_pub.string() << "\n";
  return 0;
}
