#include "client_config_crypto.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "client_config.h"
#include "secure_buffer.h"

namespace mi::client {
namespace {

struct ConfigToolOptions {
  std::filesystem::path config_path{"config/client_config.ini"};
  std::string server{"127.0.0.1"};
  std::string port{"9000"};
  std::string tls_mode{"pin"};
  std::string pinned_fingerprint;
  std::string ca_bundle;
  std::string trust_store{"server_trust.ini"};
  std::string kt_root_pub{"kt_root_pub.bin"};
  bool non_interactive{false};
  bool check{false};
  std::string expect_server;
  std::string expect_port;
  std::string expect_pin;
};

std::string ToLower(std::string s) {
  for (char& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

bool IsHexSha256(const std::string& value) {
  if (value.size() != 64) {
    return false;
  }
  return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
    return std::isxdigit(ch) != 0;
  });
}

bool ParsePort(const std::string& text, std::uint16_t& out) {
  if (text.empty()) {
    return false;
  }
  char* end_ptr = nullptr;
  const long value = std::strtol(text.c_str(), &end_ptr, 10);
  if (end_ptr == text.c_str() || *end_ptr != '\0' || value <= 0 ||
      value > 65535) {
    return false;
  }
  out = static_cast<std::uint16_t>(value);
  return true;
}

void PrintUsage(std::ostream& out) {
  out << "Usage: mi_e2ee_client_config_tool --config PATH [options]\n"
      << "Options:\n"
      << "  --server HOST_OR_IP\n"
      << "  --port PORT\n"
      << "  --tls-mode pin|ca|hybrid\n"
      << "  --pinned-fingerprint SHA256_HEX\n"
      << "  --ca-bundle PATH\n"
      << "  --trust-store PATH\n"
      << "  --kt-root-pub PATH\n"
      << "  --non-interactive\n"
      << "  --check --expect-server HOST --expect-port PORT [--expect-pin SHA256]\n";
}

bool TakeValue(int& i, int argc, char** argv, std::string& out,
               std::string& error) {
  if (i + 1 >= argc) {
    error = std::string("missing value for ") + argv[i];
    return false;
  }
  out = argv[++i];
  return true;
}

bool ParseArgs(int argc, char** argv, ConfigToolOptions& opt,
               std::string& error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i] ? argv[i] : "";
    if (arg == "--help" || arg == "-h") {
      PrintUsage(std::cout);
      return false;
    }
    if (arg == "--config") {
      std::string value;
      if (!TakeValue(i, argc, argv, value, error)) return false;
      opt.config_path = value;
    } else if (arg == "--server") {
      if (!TakeValue(i, argc, argv, opt.server, error)) return false;
    } else if (arg == "--port") {
      if (!TakeValue(i, argc, argv, opt.port, error)) return false;
    } else if (arg == "--tls-mode") {
      if (!TakeValue(i, argc, argv, opt.tls_mode, error)) return false;
    } else if (arg == "--pinned-fingerprint") {
      if (!TakeValue(i, argc, argv, opt.pinned_fingerprint, error)) {
        return false;
      }
    } else if (arg == "--ca-bundle") {
      if (!TakeValue(i, argc, argv, opt.ca_bundle, error)) return false;
    } else if (arg == "--trust-store") {
      if (!TakeValue(i, argc, argv, opt.trust_store, error)) return false;
    } else if (arg == "--kt-root-pub") {
      if (!TakeValue(i, argc, argv, opt.kt_root_pub, error)) return false;
    } else if (arg == "--non-interactive") {
      opt.non_interactive = true;
    } else if (arg == "--check") {
      opt.check = true;
    } else if (arg == "--expect-server") {
      if (!TakeValue(i, argc, argv, opt.expect_server, error)) return false;
    } else if (arg == "--expect-port") {
      if (!TakeValue(i, argc, argv, opt.expect_port, error)) return false;
    } else if (arg == "--expect-pin") {
      if (!TakeValue(i, argc, argv, opt.expect_pin, error)) return false;
    } else {
      error = "unknown argument: " + arg;
      return false;
    }
  }
  return true;
}

bool PromptField(const char* label, const std::string& fallback,
                 EncryptedConfigField& field, std::string& error) {
  std::cout << label << " [" << fallback << "]: ";
  std::string value;
  std::getline(std::cin, value);
  if (value.empty()) {
    value = fallback;
  }
  mi::common::ScopedWipe wipe(value);
  return field.SetPlain(value, error);
}

bool SetField(const std::string& value, EncryptedConfigField& field,
              std::string& error) {
  std::string local = value;
  mi::common::ScopedWipe wipe(local);
  return field.SetPlain(local, error);
}

void AppendLine(std::string& out, const std::string& line) {
  out += line;
  out.push_back('\n');
}

bool AppendFieldLine(std::string& out, const char* key,
                     const EncryptedConfigField& field, std::string& error) {
  std::string value;
  if (!field.Reveal(value, error)) {
    return false;
  }
  mi::common::ScopedWipe wipe(value);
  out += key;
  out.push_back('=');
  out += value;
  out.push_back('\n');
  return true;
}

bool BuildEncryptedFields(const ConfigToolOptions& opt,
                          EncryptedConfigField& server,
                          EncryptedConfigField& port,
                          EncryptedConfigField& tls_mode,
                          EncryptedConfigField& pin,
                          EncryptedConfigField& ca_bundle,
                          EncryptedConfigField& trust_store,
                          EncryptedConfigField& kt_root_pub,
                          std::string& error) {
  if (opt.non_interactive) {
    return SetField(opt.server, server, error) &&
           SetField(opt.port, port, error) &&
           SetField(ToLower(opt.tls_mode), tls_mode, error) &&
           SetField(opt.pinned_fingerprint, pin, error) &&
           SetField(opt.ca_bundle, ca_bundle, error) &&
           SetField(opt.trust_store, trust_store, error) &&
           SetField(opt.kt_root_pub, kt_root_pub, error);
  }

  std::cout << "MI E2EE Client Configuration\n\n";
  return PromptField("Server host/IP", opt.server, server, error) &&
         PromptField("Server port", opt.port, port, error) &&
         PromptField("TLS mode (pin|ca|hybrid)", opt.tls_mode, tls_mode,
                     error) &&
         PromptField("Pinned fingerprint", opt.pinned_fingerprint, pin,
                     error) &&
         PromptField("CA bundle path", opt.ca_bundle, ca_bundle, error) &&
         PromptField("Trust store path", opt.trust_store, trust_store,
                     error) &&
         PromptField("KT root pubkey path", opt.kt_root_pub, kt_root_pub,
                     error);
}

bool BuildPlainConfig(const EncryptedConfigField& server,
                      const EncryptedConfigField& port,
                      const EncryptedConfigField& tls_mode,
                      const EncryptedConfigField& pin,
                      const EncryptedConfigField& ca_bundle,
                      const EncryptedConfigField& trust_store,
                      const EncryptedConfigField& kt_root_pub,
                      std::string& out,
                      std::string& error) {
  std::string mode;
  if (!tls_mode.Reveal(mode, error)) return false;
  mi::common::ScopedWipe mode_wipe(mode);
  mode = ToLower(mode);
  if (mode != "pin" && mode != "ca" && mode != "hybrid") {
    error = "invalid tls mode: " + mode;
    return false;
  }

  std::string port_text;
  if (!port.Reveal(port_text, error)) return false;
  mi::common::ScopedWipe port_wipe(port_text);
  std::uint16_t parsed_port = 0;
  if (!ParsePort(port_text, parsed_port)) {
    error = "invalid server port";
    return false;
  }

  std::string fingerprint;
  if (!pin.Reveal(fingerprint, error)) return false;
  mi::common::ScopedWipe fp_wipe(fingerprint);
  fingerprint = ToLower(fingerprint);
  if ((mode == "pin" || mode == "hybrid") && !IsHexSha256(fingerprint)) {
    error = "pinned fingerprint must be 64 hex chars for pin/hybrid mode";
    return false;
  }
  if (mode == "ca") {
    fingerprint.clear();
  }
  const bool require_pin = (mode == "pin");

  std::string cfg;
  mi::common::ScopedWipe cfg_wipe(cfg);
  AppendLine(cfg, "[client]");
  if (!AppendFieldLine(cfg, "server_ip", server, error)) return false;
  AppendLine(cfg, "server_port=" + std::to_string(parsed_port));
  AppendLine(cfg, "use_tls=1");
  AppendLine(cfg, "require_tls=1");
  if (!AppendFieldLine(cfg, "trust_store", trust_store, error)) return false;
  AppendLine(cfg,
             std::string("require_pinned_fingerprint=") +
                 (require_pin ? "1" : "0"));
  AppendLine(cfg, "pinned_fingerprint=" + fingerprint);
  AppendLine(cfg, "tls_verify_mode=" + mode);
  if (!AppendFieldLine(cfg, "tls_ca_bundle_path", ca_bundle, error)) {
    return false;
  }
  AppendLine(cfg, "tls_verify_hostname=1");
  AppendLine(cfg, "auth_mode=opaque");
  AppendLine(cfg, "allow_legacy_login=0");
  AppendLine(cfg, "");
  AppendLine(cfg, "[proxy]");
  AppendLine(cfg, "type=none");
  AppendLine(cfg, "host=");
  AppendLine(cfg, "port=0");
  AppendLine(cfg, "username=");
  AppendLine(cfg, "password=");
  AppendLine(cfg, "");
  AppendLine(cfg, "[device_sync]");
  AppendLine(cfg, "enabled=1");
  AppendLine(cfg, "role=primary");
  AppendLine(cfg, "key_path=e2ee_state/device_sync_key.bin");
  AppendLine(cfg, "ratchet_enable=1");
  AppendLine(cfg, "");
  AppendLine(cfg, "[traffic]");
  AppendLine(cfg, "cover_traffic_mode=auto");
  AppendLine(cfg, "");
  AppendLine(cfg, "[kt]");
  AppendLine(cfg, "require_signature=1");
  if (!AppendFieldLine(cfg, "root_pubkey_path", kt_root_pub, error)) {
    return false;
  }

  out = std::move(cfg);
  return true;
}

bool CheckConfig(const ConfigToolOptions& opt, std::string& error) {
  std::ifstream in(opt.config_path, std::ios::binary);
  if (!in.is_open()) {
    error = "config missing: " + opt.config_path.string();
    return false;
  }
  std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  if (!IsEncryptedClientConfig(raw)) {
    error = "client config is not encrypted";
    return false;
  }

  ClientConfig cfg;
  if (!LoadClientConfig(opt.config_path.string(), cfg, error)) {
    return false;
  }
  if (!opt.expect_server.empty() && cfg.server_ip != opt.expect_server) {
    error = "server host mismatch";
    return false;
  }
  if (!opt.expect_port.empty()) {
    std::uint16_t expected = 0;
    if (!ParsePort(opt.expect_port, expected) || cfg.server_port != expected) {
      error = "server port mismatch";
      return false;
    }
  }
  if (!opt.expect_pin.empty() &&
      ToLower(cfg.pinned_fingerprint) != ToLower(opt.expect_pin)) {
    error = "pinned fingerprint mismatch";
    return false;
  }
  return true;
}

}  // namespace

int RunClientConfigTool(int argc, char** argv) {
  ConfigToolOptions opt;
  std::string error;
  if (!ParseArgs(argc, argv, opt, error)) {
    if (!error.empty()) {
      std::cerr << "error: " << error << "\n";
      PrintUsage(std::cerr);
      return 2;
    }
    return 0;
  }

  if (opt.check) {
    if (!CheckConfig(opt, error)) {
      std::cerr << "error: " << error << "\n";
      return 1;
    }
    std::cout << "Encrypted client configuration OK\n";
    return 0;
  }

  EncryptedConfigField server;
  EncryptedConfigField port;
  EncryptedConfigField tls_mode;
  EncryptedConfigField pin;
  EncryptedConfigField ca_bundle;
  EncryptedConfigField trust_store;
  EncryptedConfigField kt_root_pub;
  if (!BuildEncryptedFields(opt, server, port, tls_mode, pin, ca_bundle,
                            trust_store, kt_root_pub, error)) {
    std::cerr << "error: " << error << "\n";
    return 1;
  }

  std::string plain;
  if (!BuildPlainConfig(server, port, tls_mode, pin, ca_bundle, trust_store,
                        kt_root_pub, plain, error)) {
    std::cerr << "error: " << error << "\n";
    return 1;
  }
  mi::common::ScopedWipe plain_wipe(plain);
  if (!WriteEncryptedClientConfigText(opt.config_path, plain, error)) {
    std::cerr << "error: " << error << "\n";
    return 1;
  }
  std::cout << "Encrypted client configuration written: "
            << opt.config_path.string() << "\n";
  return 0;
}

}  // namespace mi::client
