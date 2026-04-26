#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

}  // namespace

int main() {
  const std::filesystem::path source = MI_E2EE_QUICK_CLIENT_SOURCE;
  const std::filesystem::path app_store_source = MI_E2EE_APP_STORE_SOURCE;
  const std::filesystem::path chat_display_store_source =
      MI_E2EE_CHAT_DISPLAY_STORE_SOURCE;
  const std::filesystem::path center_pane_source = MI_E2EE_CENTER_PANE_SOURCE;
  std::string body;
  if (!ReadFile(source, body)) {
    std::cerr << "missing QuickClient source: " << source << "\n";
    return 1;
  }
  std::string app_store;
  if (!ReadFile(app_store_source, app_store)) {
    std::cerr << "missing AppStore source: " << app_store_source << "\n";
    return 1;
  }
  std::string chat_display_store;
  if (!ReadFile(chat_display_store_source, chat_display_store)) {
    std::cerr << "missing ChatDisplayStore source: "
              << chat_display_store_source << "\n";
    return 1;
  }
  std::string center_pane;
  if (!ReadFile(center_pane_source, center_pane)) {
    std::cerr << "missing CenterPane source: " << center_pane_source << "\n";
    return 1;
  }
  if (!Contains(body, "#include \"protected_text_vm.h\"") ||
      !Contains(body, "WithProtectedUiText(") ||
      !Contains(body, "UiProtectedText::Protect(") ||
      !Contains(body, "kUiPlaintextLease")) {
    std::cerr << "Quick UI text path is not routed through protected VM leases\n";
    return 1;
  }
  if (Contains(body, "msg.insert(QStringLiteral(\"text\"), message);")) {
    std::cerr << "sendText still inserts a persistent plaintext QString directly\n";
    return 1;
  }
  if (Contains(body, "msg.insert(QStringLiteral(\"text\"), text);")) {
    std::cerr << "poll paths still insert decrypted text without a lease boundary\n";
    return 1;
  }
  if (Contains(body, "entry.text ? QString::fromUtf8(entry.text) : QString()")) {
    std::cerr << "history replay still inserts decoded text without a lease boundary\n";
    return 1;
  }
  if (!Contains(body, "renderProtectedText(") ||
      !Contains(body, "protectedTextId") ||
      !Contains(body, "StoreProtectedUiText(")) {
    std::cerr << "QuickClient does not expose display-time protected text handles\n";
    return 1;
  }
  if (!Contains(app_store, "function renderProtectedText(") ||
      !Contains(app_store, "protectedTextId: protectedTextId") ||
      !Contains(app_store, "text: protectedTextId.length > 0 ? \"\" : text")) {
    std::cerr << "AppStore persists protected plaintext instead of a handle\n";
    return 1;
  }
  if (Contains(app_store, "updateDialogPreview(convId, text, timeText")) {
    std::cerr << "dialog preview still stores protected plaintext directly\n";
    return 1;
  }
  if (!Contains(chat_display_store, "function messageText(entry)") ||
      !Contains(chat_display_store, "return Ui.AppStore.messageText(entry)")) {
    std::cerr << "ChatDisplayStore does not expose display-time text rendering\n";
    return 1;
  }
  if (!Contains(center_pane, "property string displayText: Ui.ChatDisplayStore.messageText(model)") ||
      Contains(center_pane, "text: model.text || \"\"")) {
    std::cerr << "CenterPane still binds visible message text to persisted model.text\n";
    return 1;
  }
  return 0;
}
