#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool read_file(const std::filesystem::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in),
             std::istreambuf_iterator<char>());
  return true;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  const std::filesystem::path source = MI_E2EE_QUICK_CLIENT_SOURCE;
  const std::filesystem::path helpers_source =
      MI_E2EE_QUICK_CLIENT_HELPERS_SOURCE;
  const std::filesystem::path app_store_source = MI_E2EE_APP_STORE_SOURCE;
  const std::filesystem::path chat_display_store_source =
      MI_E2EE_CHAT_DISPLAY_STORE_SOURCE;
  const std::filesystem::path center_pane_source = MI_E2EE_CENTER_PANE_SOURCE;
  std::string body;
  if (!read_file(source, body)) {
    std::cerr << "missing QuickClient source: " << source << "\n";
    return 1;
  }
  std::string helpers;
  if (!read_file(helpers_source, helpers)) {
    std::cerr << "missing QuickClient helpers source: " << helpers_source
              << "\n";
    return 1;
  }
  body += helpers;
  std::string app_store;
  if (!read_file(app_store_source, app_store)) {
    std::cerr << "missing AppStore source: " << app_store_source << "\n";
    return 1;
  }
  std::string chat_display_store;
  if (!read_file(chat_display_store_source, chat_display_store)) {
    std::cerr << "missing ChatDisplayStore source: "
              << chat_display_store_source << "\n";
    return 1;
  }
  std::string center_pane;
  if (!read_file(center_pane_source, center_pane)) {
    std::cerr << "missing CenterPane source: " << center_pane_source << "\n";
    return 1;
  }
  if (!contains(body, "#include \"protected_text_vm.h\"") ||
      !contains(body, "with_protected_ui_text(") ||
      !contains(body, "UiProtectedText::Protect(") ||
      !contains(body, "kUiPlaintextLease")) {
    std::cerr << "Quick UI text path is not routed through protected VM leases\n";
    return 1;
  }
  if (contains(body, "msg.insert(QStringLiteral(\"text\"), message);")) {
    std::cerr << "sendText still inserts a persistent plaintext QString directly\n";
    return 1;
  }
  if (contains(body, "msg.insert(QStringLiteral(\"text\"), text);")) {
    std::cerr << "poll paths still insert decrypted text without a lease boundary\n";
    return 1;
  }
  if (contains(body, "entry.text ? QString::fromUtf8(entry.text) : QString()")) {
    std::cerr << "history replay still inserts decoded text without a lease boundary\n";
    return 1;
  }
  if (!contains(body, "renderProtectedText(") ||
      !contains(body, "protectedTextId") ||
      !contains(body, "StoreProtectedUiText(")) {
    std::cerr << "QuickClient does not expose display-time protected text handles\n";
    return 1;
  }
  if (!contains(app_store, "function renderProtectedText(") ||
      !contains(app_store, "protectedTextId: protectedTextId") ||
      !contains(app_store, "text: protectedTextId.length > 0 ? \"\" : text")) {
    std::cerr << "AppStore persists protected plaintext instead of a handle\n";
    return 1;
  }
  if (contains(app_store, "updateDialogPreview(convId, text, timeText")) {
    std::cerr << "dialog preview still stores protected plaintext directly\n";
    return 1;
  }
  if (!contains(chat_display_store, "function messageText(entry)") ||
      !contains(chat_display_store, "return Ui.AppStore.messageText(entry)")) {
    std::cerr << "ChatDisplayStore does not expose display-time text rendering\n";
    return 1;
  }
  if (!contains(center_pane, "property string displayText: Ui.ChatDisplayStore.messageText(model)") ||
      contains(center_pane, "text: model.text || \"\"")) {
    std::cerr << "CenterPane still binds visible message text to persisted model.text\n";
    return 1;
  }
  return 0;
}
