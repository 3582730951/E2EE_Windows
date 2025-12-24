#include <windows.h>

#include <iostream>
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

std::wstring QuoteArg(const std::wstring &arg) {
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out;
    out.reserve(arg.size() + 2);
    out.push_back(L'"');
    for (const wchar_t ch : arg) {
        if (ch == L'"') {
            out.append(L"\\\"");
        } else {
            out.push_back(ch);
        }
    }
    out.push_back(L'"');
    return out;
}

}  // namespace

int wmain(int argc, wchar_t **argv) {
    const std::wstring rootDir = GetModuleDir();
    if (rootDir.empty()) {
        std::wcerr << L"[mi_e2ee_server_launcher] failed to resolve launcher directory\n";
        return 1;
    }

    const std::wstring dllDir = rootDir + L"\\dll";
    const std::wstring appExe = rootDir + L"\\mi_e2ee_server_app.exe";
    if (!FileExists(appExe)) {
        std::wcerr << L"[mi_e2ee_server_launcher] missing server binary: " << appExe << L"\n";
        return 2;
    }

    if (DirExists(dllDir)) {
        PrependPath(dllDir);
    }

    std::wstring command = L"\"";
    command.append(appExe);
    command.append(L"\"");
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            command.append(L" ");
            command.append(QuoteArg(argv[i] ? argv[i] : L""));
        }
    } else {
        const std::wstring configPath = rootDir + L"\\config\\config.ini";
        if (FileExists(configPath)) {
            command.append(L" \"config\\config.ini\"");
        }
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
        const DWORD last = GetLastError();
        std::wcerr << L"[mi_e2ee_server_launcher] failed to launch: " << appExe
                   << L" (error " << last << L")\n";
        return 3;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
