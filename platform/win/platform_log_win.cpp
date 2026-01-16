#include "platform_log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace mi::platform::log {

namespace {

std::mutex g_log_mutex;
LogCallback g_log_cb = nullptr;
void* g_log_user = nullptr;

const char* LevelToString(Level level) {
  switch (level) {
    case Level::kDebug:
      return "DEBUG";
    case Level::kInfo:
      return "INFO";
    case Level::kWarn:
      return "WARN";
    case Level::kError:
      return "ERROR";
  }
  return "INFO";
}

bool IsDelimiter(char ch) {
  const unsigned char uc = static_cast<unsigned char>(ch);
  return std::isspace(uc) != 0 || ch == ',' || ch == ';';
}

std::string ToLowerAscii(std::string_view value) {
  std::string out(value);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string RedactInline(std::string_view message) {
  std::string out(message);
  std::string lower = ToLowerAscii(out);
  static constexpr const char* kKeys[] = {
      "token", "password", "secret", "key", "pin", "device_id", "deviceid"};
  for (const char* key : kKeys) {
    const std::string pattern = std::string(key) + "=";
    std::size_t pos = 0;
    while (true) {
      pos = lower.find(pattern, pos);
      if (pos == std::string::npos) {
        break;
      }
      std::size_t start = pos + pattern.size();
      std::size_t end = start;
      while (end < out.size() && !IsDelimiter(out[end])) {
        ++end;
      }
      if (end > start) {
        out.replace(start, end - start, "***");
        lower.replace(start, end - start, "***");
        pos = start + 3;
      } else {
        pos = start;
      }
    }
  }
  return out;
}

void DefaultLog(Level level,
                std::string_view tag,
                std::string_view message,
                const Field* fields,
                std::size_t field_count) {
  std::string line;
  line.reserve(64 + message.size() + field_count * 16);
  line.append("[mi_e2ee] ");
  line.append(LevelToString(level));
  if (!tag.empty()) {
    line.push_back(' ');
    line.append(tag.data(), tag.size());
  }
  line.append(": ");
  const std::string redacted = RedactInline(message);
  line.append(redacted);
  for (std::size_t i = 0; i < field_count; ++i) {
    const auto& field = fields[i];
    if (field.key.empty()) {
      continue;
    }
    line.push_back(' ');
    line.append(field.key.data(), field.key.size());
    line.push_back('=');
    const std::string value = RedactValue(field.key, field.value);
    line.append(value);
  }
  line.push_back('\n');
  FILE* out = (level == Level::kError || level == Level::kWarn) ? stderr : stdout;
  std::fwrite(line.data(), 1, line.size(), out);
  std::fflush(out);
  OutputDebugStringA(line.c_str());
}

}  // namespace

void SetLogCallback(LogCallback cb, void* user_data) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  g_log_cb = cb;
  g_log_user = user_data;
}

void Log(Level level, std::string_view tag, std::string_view message) {
  Log(level, tag, message, {});
}

void Log(Level level,
         std::string_view tag,
         std::string_view message,
         std::initializer_list<Field> fields) {
  LogCallback cb = nullptr;
  void* user = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    cb = g_log_cb;
    user = g_log_user;
  }

  std::string tag_copy(tag);
  std::string msg_copy = RedactMessage(message);
  std::vector<std::string> redacted_values;
  std::vector<Field> safe_fields;
  if (!fields.size()) {
    if (cb) {
      cb(level, tag_copy.c_str(), msg_copy.c_str(), nullptr, 0, user);
    } else {
      DefaultLog(level, tag_copy, msg_copy, nullptr, 0);
    }
    return;
  }

  redacted_values.reserve(fields.size());
  safe_fields.reserve(fields.size());
  for (const auto& field : fields) {
    const std::string value = RedactValue(field.key, field.value);
    redacted_values.push_back(value);
    safe_fields.push_back(Field{field.key, redacted_values.back()});
  }

  if (cb) {
    cb(level, tag_copy.c_str(), msg_copy.c_str(), safe_fields.data(),
       safe_fields.size(), user);
    return;
  }
  DefaultLog(level, tag_copy, msg_copy, safe_fields.data(), safe_fields.size());
}

bool IsSensitiveKey(std::string_view key) {
  if (key.empty()) {
    return false;
  }
  const std::string lower = ToLowerAscii(key);
  if (lower.find("token") != std::string::npos ||
      lower.find("password") != std::string::npos ||
      lower.find("secret") != std::string::npos ||
      lower.find("pin") != std::string::npos ||
      lower.find("device_id") != std::string::npos ||
      lower.find("deviceid") != std::string::npos) {
    return true;
  }
  const auto key_pos = lower.find("key");
  if (key_pos != std::string::npos) {
    if (lower.find("key_id") != std::string::npos ||
        lower.find("keyid") != std::string::npos) {
      return false;
    }
    return true;
  }
  return false;
}

std::string RedactValue(std::string_view key, std::string_view value) {
  if (IsSensitiveKey(key)) {
    return "***";
  }
  return std::string(value);
}

std::string RedactMessage(std::string_view message) {
  return RedactInline(message);
}

}  // namespace mi::platform::log
