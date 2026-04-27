#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include "api_service.h"
#include "auth_provider.h"
#include "group_call_manager.h"
#include "group_directory.h"
#include "offline_storage.h"
#include "session_manager.h"
#include "test_auth_helpers.h"

namespace {

using mi::server::ApiService;
using mi::server::DemoAuthProvider;
using mi::server::DemoUserTable;
using mi::server::GroupCallManager;
using mi::server::GroupDirectory;
using mi::server::GroupManager;
using mi::server::OfflineQueue;
using mi::server::OfflineStorage;
using mi::server::Session;
using mi::server::SessionManager;
using mi::server::TransportKind;

struct Config {
  std::string scenario{"all"};
  std::uint32_t clients{32};
  std::uint32_t messages{200};
  std::size_t payload_size{1024};
  std::size_t file_size{8u * 1024u * 1024u};
  std::size_t chunk_size{1024u * 1024u};
  std::filesystem::path output_dir{"stress_results"};
};

struct ScenarioResult {
  std::string name;
  bool ok{false};
  std::uint64_t attempted{0};
  std::uint64_t delivered{0};
  double elapsed_sec{0.0};
  double throughput{0.0};
  std::string unit{"ops/s"};
  std::string error;
};

struct Context {
  std::filesystem::path base_dir;
  std::unique_ptr<SessionManager> sessions;
  std::unique_ptr<GroupManager> groups;
  std::unique_ptr<GroupCallManager> calls;
  std::unique_ptr<GroupDirectory> directory;
  std::unique_ptr<OfflineStorage> storage;
  std::unique_ptr<OfflineQueue> queue;
  std::unique_ptr<ApiService> api;
  std::vector<Session> logged_in;
  std::vector<std::string> usernames;
};

double SecondsSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::duration<double>>(
             std::chrono::steady_clock::now() - start)
      .count();
}

std::string JsonEscape(const std::string& value) {
  std::ostringstream out;
  for (char c : value) {
    switch (c) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << c;
        break;
    }
  }
  return out.str();
}

std::uint64_t PeakRssKb() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
    return static_cast<std::uint64_t>(counters.PeakWorkingSetSize / 1024);
  }
  return 0;
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    return static_cast<std::uint64_t>(usage.ru_maxrss);
  }
  return 0;
#endif
}

std::filesystem::path TempBase() {
  const auto now = std::chrono::system_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("mi_e2ee_business_stress_" + std::to_string(now));
}

std::vector<std::uint8_t> Payload(std::size_t size, std::uint8_t seed) {
  std::vector<std::uint8_t> out(size);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::uint8_t>((i + seed) & 0xFFu);
  }
  return out;
}

bool MakeContext(const Config& cfg, Context& ctx, std::string& error) {
  ctx.base_dir = TempBase();
  std::error_code ec;
  std::filesystem::remove_all(ctx.base_dir, ec);
  std::filesystem::create_directories(ctx.base_dir, ec);
  if (ec) {
    error = "failed to create temp dir";
    return false;
  }

  DemoUserTable users;
  for (std::uint32_t i = 0; i < cfg.clients; ++i) {
    const std::string username = "user" + std::to_string(i);
    users.emplace(username,
                  mi::server::test::MakeDemoUser(username, "password"));
    ctx.usernames.push_back(username);
  }

  ctx.sessions =
      std::make_unique<SessionManager>(std::make_unique<DemoAuthProvider>(
          std::move(users)));
  ctx.groups = std::make_unique<GroupManager>(ctx.base_dir / "groups");
  ctx.calls = std::make_unique<GroupCallManager>();
  ctx.directory = std::make_unique<GroupDirectory>(ctx.base_dir / "directory");
  ctx.storage =
      std::make_unique<OfflineStorage>(ctx.base_dir / "offline",
                                       std::chrono::seconds(3600));
  ctx.queue = std::make_unique<OfflineQueue>(std::chrono::seconds(3600));
  ctx.api = std::make_unique<ApiService>(
      ctx.sessions.get(), ctx.groups.get(), ctx.calls.get(),
      ctx.directory.get(), ctx.storage.get(), ctx.queue.get());

  for (const auto& username : ctx.usernames) {
    Session session;
    std::string login_error;
    if (!ctx.sessions->Login(username, "password", TransportKind::kLocal,
                             session, login_error)) {
      error = "login failed: " + username + ": " + login_error;
      return false;
    }
    ctx.logged_in.push_back(std::move(session));
  }
  return true;
}

ScenarioResult RunPrivateChat(const Config& cfg, Context& ctx) {
  ScenarioResult result;
  result.name = "private_chat_throughput";
  const auto start = std::chrono::steady_clock::now();
  const auto payload = Payload(cfg.payload_size, 0x21);

  for (std::uint32_t i = 0; i < cfg.clients; ++i) {
    const std::uint32_t recipient = (i + 1) % cfg.clients;
    auto add = ctx.api->AddFriend(ctx.logged_in[i].token,
                                  ctx.usernames[recipient]);
    if (!add.success) {
      result.error = "AddFriend failed";
      return result;
    }
  }

  for (std::uint32_t m = 0; m < cfg.messages; ++m) {
    for (std::uint32_t i = 0; i < cfg.clients; ++i) {
      const std::uint32_t recipient = (i + 1) % cfg.clients;
      auto sent = ctx.api->SendPrivate(ctx.logged_in[i].token,
                                       ctx.usernames[recipient], payload);
      ++result.attempted;
      if (!sent.success) {
        result.error = "SendPrivate failed";
        return result;
      }
    }
  }

  for (std::uint32_t i = 0; i < cfg.clients; ++i) {
    auto pulled = ctx.api->PullPrivate(ctx.logged_in[i].token);
    if (!pulled.success) {
      result.error = "PullPrivate failed";
      return result;
    }
    result.delivered += pulled.messages.size();
  }

  result.elapsed_sec = SecondsSince(start);
  result.throughput = result.elapsed_sec > 0.0
                          ? static_cast<double>(result.delivered) /
                                result.elapsed_sec
                          : 0.0;
  result.ok = result.delivered == result.attempted;
  if (!result.ok) {
    result.error = "private delivered count mismatch";
  }
  return result;
}

ScenarioResult RunGroupChat(const Config& cfg, Context& ctx) {
  ScenarioResult result;
  result.name = "group_chat_throughput";
  const auto start = std::chrono::steady_clock::now();
  const auto payload = Payload(cfg.payload_size, 0x41);
  const std::string group_id = "stress_group";

  for (std::uint32_t i = 0; i < cfg.clients; ++i) {
    auto join = ctx.api->JoinGroup(ctx.logged_in[i].token, group_id);
    if (!join.success) {
      result.error = "JoinGroup failed";
      return result;
    }
  }

  for (std::uint32_t m = 0; m < cfg.messages; ++m) {
    auto sent = ctx.api->SendGroupCipher(ctx.logged_in[0].token, group_id,
                                         payload);
    ++result.attempted;
    if (!sent.success) {
      result.error = "SendGroupCipher failed";
      return result;
    }
  }

  for (std::uint32_t i = 1; i < cfg.clients; ++i) {
    auto pulled = ctx.api->PullGroupCipher(ctx.logged_in[i].token);
    if (!pulled.success) {
      result.error = "PullGroupCipher failed";
      return result;
    }
    result.delivered += pulled.messages.size();
  }

  result.attempted *= (cfg.clients > 0 ? cfg.clients - 1 : 0);
  result.elapsed_sec = SecondsSince(start);
  result.throughput = result.elapsed_sec > 0.0
                          ? static_cast<double>(result.delivered) /
                                result.elapsed_sec
                          : 0.0;
  result.ok = result.delivered == result.attempted;
  if (!result.ok) {
    result.error = "group delivered count mismatch";
  }
  return result;
}

ScenarioResult RunOfflineFile(const Config& cfg, Context& ctx) {
  ScenarioResult result;
  result.name = "offline_file_upload_download";
  result.unit = "MB/s";
  const auto start = std::chrono::steady_clock::now();
  const std::uint32_t files = std::max<std::uint32_t>(1, cfg.messages);
  const auto chunk_size = std::max<std::size_t>(1, cfg.chunk_size);
  const auto chunk = Payload(chunk_size, 0x77);

  std::uint64_t bytes_done = 0;
  for (std::uint32_t file = 0; file < files; ++file) {
    const std::uint32_t owner = file % cfg.clients;
    const std::string& owner_token = ctx.logged_in[owner].token;
    auto upload = ctx.api->StartE2eeFileBlobUpload(owner_token,
                                                   cfg.file_size);
    ++result.attempted;
    if (!upload.success) {
      result.error = "StartE2eeFileBlobUpload failed: " + upload.error;
      return result;
    }
    std::uint64_t offset = 0;
    while (offset < cfg.file_size) {
      const std::size_t n =
          static_cast<std::size_t>(std::min<std::uint64_t>(
              chunk_size, static_cast<std::uint64_t>(cfg.file_size - offset)));
      std::vector<std::uint8_t> part(chunk.begin(), chunk.begin() + n);
      auto appended = ctx.api->UploadE2eeFileBlobChunk(
          owner_token, upload.file_id, upload.upload_id, offset,
          part);
      if (!appended.success) {
        result.error = "UploadE2eeFileBlobChunk failed: " + appended.error;
        return result;
      }
      offset += n;
    }
    auto finished = ctx.api->FinishE2eeFileBlobUpload(
        owner_token, upload.file_id, upload.upload_id,
        cfg.file_size);
    if (!finished.success) {
      result.error = "FinishE2eeFileBlobUpload failed: " + finished.error;
      return result;
    }

    auto download = ctx.api->StartE2eeFileBlobDownload(
        owner_token, upload.file_id, true);
    if (!download.success) {
      result.error = "StartE2eeFileBlobDownload failed: " + download.error;
      return result;
    }
    offset = 0;
    while (true) {
      auto read = ctx.api->DownloadE2eeFileBlobChunk(
          owner_token, upload.file_id, download.download_id, offset,
          static_cast<std::uint32_t>(std::min<std::size_t>(
              chunk_size, 8u * 1024u * 1024u)));
      if (!read.success) {
        result.error = "DownloadE2eeFileBlobChunk failed: " + read.error;
        return result;
      }
      offset += read.chunk.size();
      if (read.eof) {
        break;
      }
    }
    if (offset != cfg.file_size) {
      result.error = "downloaded byte count mismatch";
      return result;
    }
    ++result.delivered;
    bytes_done += cfg.file_size * 2u;
  }

  result.elapsed_sec = SecondsSince(start);
  result.throughput =
      result.elapsed_sec > 0.0
          ? (static_cast<double>(bytes_done) / (1024.0 * 1024.0)) /
                result.elapsed_sec
          : 0.0;
  result.ok = result.delivered == result.attempted;
  return result;
}

Config MixedConfig(const Config& cfg) {
  Config mixed = cfg;
  mixed.clients = std::max<std::uint32_t>(2, std::min<std::uint32_t>(cfg.clients, 16));
  mixed.messages = std::max<std::uint32_t>(1, cfg.messages / 4);
  mixed.file_size = std::max<std::size_t>(1024u * 1024u, cfg.file_size / 2);
  return mixed;
}

ScenarioResult RunMixed(const Config& cfg) {
  ScenarioResult result;
  result.name = "mixed_scenario";
  const auto start = std::chrono::steady_clock::now();
  const Config mixed_cfg = MixedConfig(cfg);

  Context private_ctx;
  std::string error;
  if (!MakeContext(mixed_cfg, private_ctx, error)) {
    result.error = error;
    return result;
  }
  auto private_result = RunPrivateChat(mixed_cfg, private_ctx);
  if (!private_result.ok) {
    result.error = private_result.error;
    return result;
  }

  Context group_ctx;
  if (!MakeContext(mixed_cfg, group_ctx, error)) {
    result.error = error;
    return result;
  }
  auto group_result = RunGroupChat(mixed_cfg, group_ctx);
  if (!group_result.ok) {
    result.error = group_result.error;
    return result;
  }

  Context file_ctx;
  if (!MakeContext(mixed_cfg, file_ctx, error)) {
    result.error = error;
    return result;
  }
  auto file_result = RunOfflineFile(mixed_cfg, file_ctx);
  if (!file_result.ok) {
    result.error = file_result.error;
    return result;
  }

  result.attempted =
      private_result.attempted + group_result.attempted + file_result.attempted;
  result.delivered =
      private_result.delivered + group_result.delivered + file_result.delivered;
  result.elapsed_sec = SecondsSince(start);
  result.throughput = result.elapsed_sec > 0.0
                          ? static_cast<double>(result.delivered) /
                                result.elapsed_sec
                          : 0.0;
  result.ok = true;
  return result;
}

ScenarioResult RunScenario(const Config& cfg, const std::string& scenario) {
  if (scenario == "mixed") {
    return RunMixed(cfg);
  }
  Context ctx;
  std::string error;
  if (!MakeContext(cfg, ctx, error)) {
    ScenarioResult failed;
    failed.name = scenario;
    failed.error = error;
    return failed;
  }
  if (scenario == "private") {
    return RunPrivateChat(cfg, ctx);
  }
  if (scenario == "group") {
    return RunGroupChat(cfg, ctx);
  }
  if (scenario == "offline") {
    return RunOfflineFile(cfg, ctx);
  }
  ScenarioResult failed;
  failed.name = scenario;
  failed.error = "unknown scenario";
  return failed;
}

std::string Timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
  return out.str();
}

bool WriteJson(const Config& cfg, const std::vector<ScenarioResult>& results,
               std::uint64_t peak_rss_kb, std::filesystem::path& out_path,
               std::string& error) {
  std::error_code ec;
  std::filesystem::create_directories(cfg.output_dir, ec);
  if (ec) {
    error = "failed to create output dir";
    return false;
  }
  out_path = cfg.output_dir / ("business_" + Timestamp() + ".json");
  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "failed to write output json";
    return false;
  }
  const bool ok =
      std::all_of(results.begin(), results.end(),
                  [](const ScenarioResult& item) { return item.ok; });
  out << "{\n";
  out << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
  out << "  \"scenario\": \"" << JsonEscape(cfg.scenario) << "\",\n";
  out << "  \"clients\": " << cfg.clients << ",\n";
  out << "  \"messages\": " << cfg.messages << ",\n";
  out << "  \"payload_size\": " << cfg.payload_size << ",\n";
  out << "  \"file_size\": " << cfg.file_size << ",\n";
  out << "  \"chunk_size\": " << cfg.chunk_size << ",\n";
  out << "  \"peak_rss_kb\": " << peak_rss_kb << ",\n";
  out << "  \"results\": [\n";
  for (std::size_t i = 0; i < results.size(); ++i) {
    const auto& r = results[i];
    out << "    {\n";
    out << "      \"name\": \"" << JsonEscape(r.name) << "\",\n";
    out << "      \"ok\": " << (r.ok ? "true" : "false") << ",\n";
    out << "      \"attempted\": " << r.attempted << ",\n";
    out << "      \"delivered\": " << r.delivered << ",\n";
    out << "      \"elapsed_sec\": " << std::fixed << std::setprecision(6)
        << r.elapsed_sec << ",\n";
    out << "      \"throughput\": " << std::fixed << std::setprecision(6)
        << r.throughput << ",\n";
    out << "      \"unit\": \"" << JsonEscape(r.unit) << "\",\n";
    out << "      \"error\": \"" << JsonEscape(r.error) << "\"\n";
    out << "    }" << (i + 1 == results.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
  return true;
}

bool ParseUint(const char* raw, std::uint64_t& out) {
  if (!raw || !*raw) {
    return false;
  }
  char* end = nullptr;
  const auto parsed = std::strtoull(raw, &end, 10);
  if (!end || *end != '\0') {
    return false;
  }
  out = parsed;
  return true;
}

void Usage(const char* argv0) {
  std::cout << "Usage: " << (argv0 ? argv0 : "mi_e2ee_business_stress")
            << " [--scenario all|private|group|offline|mixed]"
            << " [--clients N] [--messages N] [--payload-size N]"
            << " [--file-size N] [--chunk-size N]"
            << " [--output-dir DIR]\n";
}

bool ParseArgs(int argc, char** argv, Config& cfg) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto need_value = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        std::cerr << name << " requires a value\n";
        return nullptr;
      }
      return argv[++i];
    };
    if (arg == "--scenario") {
      const char* value = need_value("--scenario");
      if (!value) return false;
      cfg.scenario = value;
    } else if (arg == "--clients") {
      const char* value = need_value("--clients");
      std::uint64_t parsed = 0;
      if (!value || !ParseUint(value, parsed) || parsed < 2 || parsed > 5000) {
        std::cerr << "invalid --clients\n";
        return false;
      }
      cfg.clients = static_cast<std::uint32_t>(parsed);
    } else if (arg == "--messages") {
      const char* value = need_value("--messages");
      std::uint64_t parsed = 0;
      if (!value || !ParseUint(value, parsed) || parsed < 1 ||
          parsed > 1000000) {
        std::cerr << "invalid --messages\n";
        return false;
      }
      cfg.messages = static_cast<std::uint32_t>(parsed);
    } else if (arg == "--payload-size") {
      const char* value = need_value("--payload-size");
      std::uint64_t parsed = 0;
      if (!value || !ParseUint(value, parsed) || parsed > 8ull * 1024ull * 1024ull) {
        std::cerr << "invalid --payload-size\n";
        return false;
      }
      cfg.payload_size = static_cast<std::size_t>(parsed);
    } else if (arg == "--file-size") {
      const char* value = need_value("--file-size");
      std::uint64_t parsed = 0;
      if (!value || !ParseUint(value, parsed) || parsed > 320ull * 1024ull * 1024ull) {
        std::cerr << "invalid --file-size\n";
        return false;
      }
      cfg.file_size = static_cast<std::size_t>(parsed);
    } else if (arg == "--chunk-size") {
      const char* value = need_value("--chunk-size");
      std::uint64_t parsed = 0;
      if (!value || !ParseUint(value, parsed) || parsed < 1 ||
          parsed > 8ull * 1024ull * 1024ull) {
        std::cerr << "invalid --chunk-size\n";
        return false;
      }
      cfg.chunk_size = static_cast<std::size_t>(parsed);
    } else if (arg == "--output-dir") {
      const char* value = need_value("--output-dir");
      if (!value) return false;
      cfg.output_dir = value;
    } else if (arg == "--help" || arg == "-h") {
      Usage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "unknown argument: " << arg << "\n";
      Usage(argv[0]);
      return false;
    }
  }
  return cfg.scenario == "all" || cfg.scenario == "private" ||
         cfg.scenario == "group" || cfg.scenario == "offline" ||
         cfg.scenario == "mixed";
}

}  // namespace

int main(int argc, char** argv) {
  Config cfg;
  if (!ParseArgs(argc, argv, cfg)) {
    return 2;
  }

  std::vector<std::string> scenarios;
  if (cfg.scenario == "all") {
    scenarios = {"private", "group", "offline", "mixed"};
  } else {
    scenarios = {cfg.scenario};
  }

  std::vector<ScenarioResult> results;
  for (const auto& scenario : scenarios) {
    auto result = RunScenario(cfg, scenario);
    results.push_back(std::move(result));
  }

  std::filesystem::path json_path;
  std::string error;
  if (!WriteJson(cfg, results, PeakRssKb(), json_path, error)) {
    std::cerr << "business stress failed: " << error << "\n";
    return 1;
  }

  const bool ok =
      std::all_of(results.begin(), results.end(),
                  [](const ScenarioResult& item) { return item.ok; });
  std::cout << "business_stress_json=" << json_path.string() << "\n";
  for (const auto& result : results) {
    std::cout << result.name << ": " << (result.ok ? "ok" : "failed")
              << " attempted=" << result.attempted
              << " delivered=" << result.delivered
              << " throughput=" << std::fixed << std::setprecision(2)
              << result.throughput << " " << result.unit;
    if (!result.error.empty()) {
      std::cout << " error=" << result.error;
    }
    std::cout << "\n";
  }
  return ok ? 0 : 1;
}
