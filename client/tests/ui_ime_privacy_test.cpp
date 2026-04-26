#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool ReadFile(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in),
             std::istreambuf_iterator<char>());
  return true;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::string StripLuaLineComments(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  std::size_t line_start = 0;
  while (line_start < input.size()) {
    std::size_t line_end = input.find('\n', line_start);
    if (line_end == std::string::npos) {
      line_end = input.size();
    }
    std::string line = input.substr(line_start, line_end - line_start);
    const std::size_t comment = line.find("--");
    if (comment != std::string::npos) {
      line.resize(comment);
    }
    out += line;
    out.push_back('\n');
    line_start = line_end + 1;
  }
  return out;
}

bool IsLuaFile(const std::filesystem::path& path) {
  return path.extension() == ".lua";
}

bool IsYamlFile(const std::filesystem::path& path) {
  const auto ext = path.extension().string();
  return ext == ".yaml" || ext == ".yml";
}

std::string StripHashLineComments(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  std::size_t line_start = 0;
  while (line_start < input.size()) {
    std::size_t line_end = input.find('\n', line_start);
    if (line_end == std::string::npos) {
      line_end = input.size();
    }
    std::string line = input.substr(line_start, line_end - line_start);
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line.resize(comment);
    }
    out += line;
    out.push_back('\n');
    line_start = line_end + 1;
  }
  return out;
}

}  // namespace

int main() {
  const std::filesystem::path rime_lua_root = MI_E2EE_RIME_LUA_ROOT;
  if (!std::filesystem::is_directory(rime_lua_root)) {
    std::cerr << "missing Rime Lua root: " << rime_lua_root << "\n";
    return 1;
  }

  const std::vector<std::string> forbidden = {
      "io.open",
      ":write(",
      "os.getenv",
      "debug.getinfo",
      "runLog",
      "runLog.txt",
      "logDoc",
      "writeLog",
      "print(",
      "log.info",
      "log.error",
      "commit_history",
      "update_userdict",
  };

  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(rime_lua_root, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate Rime Lua root: " << ec.message()
                << "\n";
      return 1;
    }
    if (!it->is_regular_file(ec) || !IsLuaFile(it->path())) {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Rime Lua source: "
                << it->path().generic_string() << "\n";
      return 1;
    }
    body = StripLuaLineComments(body);
    for (const auto& needle : forbidden) {
      if (Contains(body, needle)) {
        std::cerr << "Rime Lua privacy marker " << needle << " in "
                  << it->path().generic_string() << "\n";
        return 1;
      }
    }
  }

  const std::filesystem::path rime_root = MI_E2EE_RIME_ROOT;
  if (!std::filesystem::is_directory(rime_root)) {
    std::cerr << "missing Rime root: " << rime_root << "\n";
    return 1;
  }
  const std::vector<std::string> forbidden_yaml = {
      "enable_user_dict: true",
      "db_class: tabledb",
  };
  for (std::filesystem::recursive_directory_iterator it(rime_root, ec), end;
       it != end; it.increment(ec)) {
    if (ec) {
      std::cerr << "failed to iterate Rime root: " << ec.message() << "\n";
      return 1;
    }
    if (!it->is_regular_file(ec) || !IsYamlFile(it->path())) {
      continue;
    }
    std::string body;
    if (!ReadFile(it->path(), body)) {
      std::cerr << "failed to read Rime source: "
                << it->path().generic_string() << "\n";
      return 1;
    }
    body = StripHashLineComments(body);
    for (const auto& needle : forbidden_yaml) {
      if (Contains(body, needle)) {
        std::cerr << "Rime schema privacy marker " << needle << " in "
                  << it->path().generic_string() << "\n";
        return 1;
      }
    }
  }

  const std::filesystem::path qml_components = MI_E2EE_QML_COMPONENT_ROOT;
  for (const auto& name : {"SecureTextField.qml", "SecureTextArea.qml"}) {
    const auto path = qml_components / name;
    std::string body;
    if (!ReadFile(path, body)) {
      std::cerr << "failed to read QML secure input component: "
                << path.generic_string() << "\n";
      return 1;
    }
    if (!Contains(body, "inputMethodHints: Qt.ImhNoPredictiveText")) {
      std::cerr << "QML secure input component missing no-predictive hint: "
                << path.generic_string() << "\n";
      return 1;
    }
  }
  return 0;
}
