#include "../common/ImePluginApi.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "third_party/rime_api.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
#if defined(_WIN32)
using LibHandle = HMODULE;
#else
using LibHandle = void *;
#endif

LibHandle gRimeLib = nullptr;
RimeApi *gApi = nullptr;
bool gInitialized = false;

std::string GetModuleDir() {
#if defined(_WIN32)
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&GetModuleDir),
                            &module)) {
        return {};
    }
    char path[MAX_PATH] = {};
    if (!GetModuleFileNameA(module, path, MAX_PATH)) {
        return {};
    }
    std::string full(path);
    const size_t pos = full.find_last_of("\\/");
    if (pos == std::string::npos) {
        return {};
    }
    return full.substr(0, pos);
#else
    Dl_info info{};
    if (dladdr(reinterpret_cast<void *>(&GetModuleDir), &info) == 0 || !info.dli_fname) {
        return {};
    }
    std::string full(info.dli_fname);
    const size_t pos = full.find_last_of('/');
    if (pos == std::string::npos) {
        return {};
    }
    return full.substr(0, pos);
#endif
}

LibHandle LoadLib(const std::string &path) {
#if defined(_WIN32)
    return LoadLibraryA(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY);
#endif
}

void *GetSymbol(LibHandle handle, const char *name) {
#if defined(_WIN32)
    return reinterpret_cast<void *>(GetProcAddress(handle, name));
#else
    return dlsym(handle, name);
#endif
}

void UnloadLib(LibHandle handle) {
#if defined(_WIN32)
    if (handle) {
        FreeLibrary(handle);
    }
#else
    if (handle) {
        dlclose(handle);
    }
#endif
}

bool LoadRime() {
    if (gRimeLib && gApi) {
        return true;
    }
    const std::string base = GetModuleDir();
    std::vector<std::string> candidates;
#if defined(_WIN32)
    if (!base.empty()) {
        candidates.push_back(base + "\\rime.dll");
        candidates.push_back(base + "\\librime.dll");
    }
    candidates.push_back("rime.dll");
    candidates.push_back("librime.dll");
#else
    if (!base.empty()) {
        candidates.push_back(base + "/librime.so");
        candidates.push_back(base + "/librime.dylib");
    }
    candidates.push_back("librime.so");
    candidates.push_back("librime.dylib");
#endif
    for (const auto &path : candidates) {
        gRimeLib = LoadLib(path);
        if (gRimeLib) {
            break;
        }
    }
    if (!gRimeLib) {
        return false;
    }
    using GetApiFn = RimeApi *(*)();
    auto *getApi = reinterpret_cast<GetApiFn>(GetSymbol(gRimeLib, "rime_get_api"));
    if (!getApi) {
        UnloadLib(gRimeLib);
        gRimeLib = nullptr;
        return false;
    }
    gApi = getApi();
    if (!gApi) {
        UnloadLib(gRimeLib);
        gRimeLib = nullptr;
        return false;
    }
    return true;
}
}  // namespace

extern "C" {
MI_IME_EXPORT int MiImeApiVersion() {
    return kMiImeApiVersion;
}

MI_IME_EXPORT bool MiImeInitialize(const char *shared_dir, const char *user_dir) {
    if (gInitialized) {
        return true;
    }
    if (!shared_dir || !user_dir) {
        return false;
    }
    if (!LoadRime()) {
        return false;
    }
    RimeTraits traits;
    RIME_STRUCT_INIT(RimeTraits, traits);
    traits.shared_data_dir = shared_dir;
    traits.user_data_dir = user_dir;
    traits.distribution_name = "mi_e2ee";
    traits.distribution_code_name = "mi_e2ee";
    traits.distribution_version = "1.0";
    traits.app_name = "rime.mi_e2ee";
    traits.min_log_level = 2;
    traits.log_dir = "";
    gApi->setup(&traits);
    gApi->initialize(&traits);
    gApi->deploy();
    gApi->join_maintenance_thread();
    gInitialized = true;
    return true;
}

MI_IME_EXPORT void MiImeShutdown() {
    if (!gInitialized) {
        return;
    }
    if (gApi && gApi->finalize) {
        gApi->finalize();
    }
    gInitialized = false;
    gApi = nullptr;
    if (gRimeLib) {
        UnloadLib(gRimeLib);
        gRimeLib = nullptr;
    }
}

MI_IME_EXPORT void *MiImeCreateSession() {
    if (!gInitialized || !gApi || !gApi->create_session) {
        return nullptr;
    }
    const RimeSessionId session = gApi->create_session();
    if (!session) {
        return nullptr;
    }
    if (gApi->select_schema) {
        gApi->select_schema(session, "mi_pinyin");
    }
    return reinterpret_cast<void *>(static_cast<uintptr_t>(session));
}

MI_IME_EXPORT void MiImeDestroySession(void *session) {
    if (!gApi || !session || !gApi->destroy_session) {
        return;
    }
    const RimeSessionId id = reinterpret_cast<RimeSessionId>(session);
    gApi->destroy_session(id);
}

MI_IME_EXPORT int MiImeGetCandidates(void *session,
                                     const char *input,
                                     char *out_buffer,
                                     size_t out_size,
                                     int max_candidates) {
    if (!gApi || !session || !input || !out_buffer || out_size == 0 ||
        max_candidates <= 0) {
        return 0;
    }
    out_buffer[0] = '\0';
    const RimeSessionId id = reinterpret_cast<RimeSessionId>(session);
    if (*input == '\0') {
        if (gApi->clear_composition) {
            gApi->clear_composition(id);
        }
        return 0;
    }
    if (!gApi->set_input || !gApi->set_input(id, input)) {
        return 0;
    }
    RimeContext ctx;
    RIME_STRUCT_INIT(RimeContext, ctx);
    if (!gApi->get_context || !gApi->get_context(id, &ctx)) {
        return 0;
    }
    const int total = std::min(ctx.menu.num_candidates, max_candidates);
    size_t remaining = out_size;
    char *cursor = out_buffer;
    int written = 0;
    for (int i = 0; i < total; ++i) {
        const char *cand = ctx.menu.candidates[i].text;
        if (!cand || !*cand) {
            continue;
        }
        if (written > 0) {
            if (remaining <= 1) {
                break;
            }
            *cursor++ = '\n';
            remaining--;
        }
        size_t len = std::strlen(cand);
        if (len >= remaining) {
            len = remaining - 1;
        }
        if (len == 0) {
            break;
        }
        std::memcpy(cursor, cand, len);
        cursor += len;
        remaining -= len;
        *cursor = '\0';
        written++;
    }
    if (gApi->free_context) {
        gApi->free_context(&ctx);
    }
    return written;
}
}  // extern "C"
