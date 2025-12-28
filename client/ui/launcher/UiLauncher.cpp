// Windows launcher to bootstrap runtime DLL search paths.
#include <windows.h>

#include <string>
#include <vector>

namespace {

std::wstring GetModuleDir() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    std::wstring full(path, len);
    const size_t pos = full.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        return {};
    }
    return full.substr(0, pos);
}

bool FileExists(const std::wstring &path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DirExists(const std::wstring &path) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring GetEnvVar(const wchar_t *name) {
    const DWORD len = GetEnvironmentVariableW(name, nullptr, 0);
    if (len == 0) {
        return {};
    }
    std::wstring value(len, L'\0');
    const DWORD read = GetEnvironmentVariableW(name, value.data(), len);
    if (read == 0 || read >= len) {
        return {};
    }
    value.resize(read);
    return value;
}

void PrependPath(const std::wstring &dir) {
    if (dir.empty()) {
        return;
    }
    const std::wstring path = GetEnvVar(L"PATH");
    std::wstring updated = dir;
    if (!path.empty()) {
        updated.append(L";");
        updated.append(path);
    }
    SetEnvironmentVariableW(L"PATH", updated.c_str());
}

void SetEnvIfEmpty(const wchar_t *name, const std::wstring &value) {
    if (value.empty()) {
        return;
    }
    const DWORD len = GetEnvironmentVariableW(name, nullptr, 0);
    if (len == 0) {
        SetEnvironmentVariableW(name, value.c_str());
    }
}

void ShowError(const std::wstring &message) {
    MessageBoxW(nullptr, message.c_str(), L"MI E2EE", MB_OK | MB_ICONERROR);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    const std::wstring rootDir = GetModuleDir();
    if (rootDir.empty()) {
        ShowError(L"Failed to resolve launcher directory.");
        return 1;
    }
    const std::wstring dllDir = rootDir + L"\\dll";
    const std::wstring appExe = rootDir + L"\\mi_e2ee_client_ui_app.exe";
    if (!FileExists(appExe)) {
        ShowError(L"Missing runtime executable: " + appExe);
        return 2;
    }
    if (DirExists(dllDir)) {
        PrependPath(dllDir);
        const std::wstring pluginRoot =
            DirExists(dllDir + L"\\plugins") ? (dllDir + L"\\plugins") : dllDir;
        const std::wstring platformDir = pluginRoot + L"\\platforms";
        const std::wstring qmlDir = dllDir + L"\\qml";
        if (DirExists(pluginRoot)) {
            SetEnvIfEmpty(L"QT_PLUGIN_PATH", pluginRoot);
        }
        if (DirExists(platformDir)) {
            SetEnvIfEmpty(L"QT_QPA_PLATFORM_PLUGIN_PATH", platformDir);
        }
        if (DirExists(qmlDir)) {
            SetEnvIfEmpty(L"QML2_IMPORT_PATH", qmlDir);
            SetEnvIfEmpty(L"QML_IMPORT_PATH", qmlDir);
        }
    }

    std::wstring command = L"\"";
    command.append(appExe);
    command.append(L"\"");
    if (cmdLine && *cmdLine) {
        command.append(L" ");
        command.append(cmdLine);
    }
    std::vector<wchar_t> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(appExe.c_str(),
                        cmdBuffer.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        rootDir.c_str(),
                        &startup,
                        &process)) {
        ShowError(L"Failed to launch: " + appExe);
        return 3;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
