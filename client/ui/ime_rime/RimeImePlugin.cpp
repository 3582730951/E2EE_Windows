#include "../common/ImePluginApi.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
std::string gPreferredSchema;

bool HasCompiledData(const std::string &user_dir) {
    if (user_dir.empty()) {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path root(user_dir);
    if (!std::filesystem::exists(root, ec)) {
        return false;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec),
              end = std::filesystem::recursive_directory_iterator();
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            continue;
        }
        if (it->path().extension() == ".bin") {
            return true;
        }
    }
    return false;
}

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

std::string LoadPreferredSchema(const char *user_dir) {
    if (!user_dir || !*user_dir) {
        return {};
    }
    std::filesystem::path path(user_dir);
    path /= "ime_schema.txt";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return {};
    }
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    std::string schema;
    std::getline(in, schema);
    while (!schema.empty() && (schema.back() == '\r' || schema.back() == '\n' || schema.back() == ' ')) {
        schema.pop_back();
    }
    return schema;
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
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = shared_dir;
    traits.user_data_dir = user_dir;
    traits.distribution_name = "mi_e2ee";
    traits.distribution_code_name = "mi_e2ee";
    traits.distribution_version = "1.0";
    traits.app_name = "rime.mi_e2ee";
    traits.min_log_level = 2;
    traits.log_dir = "";
    gApi->setup(&traits);
    if (gApi->deployer_initialize) {
        gApi->deployer_initialize(&traits);
    }
    gApi->initialize(&traits);
    const bool hasCompiled = HasCompiledData(user_dir);
    bool forceDeploy = false;
    if (const char *env = std::getenv("MI_E2EE_RIME_FORCE_DEPLOY")) {
        if (*env != '\0' && std::strcmp(env, "0") != 0) {
            forceDeploy = true;
        }
    }
    const bool needsDeploy = !hasCompiled || forceDeploy;
    bool maintenanceStarted = false;
    if (needsDeploy && gApi->start_maintenance) {
        maintenanceStarted = gApi->start_maintenance(True) == True;
    }
    if (!maintenanceStarted && needsDeploy && gApi->deploy) {
        gApi->deploy();
    }
    if (maintenanceStarted && gApi->join_maintenance_thread) {
        gApi->join_maintenance_thread();
    }
    gPreferredSchema = LoadPreferredSchema(user_dir);
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
        if (!gPreferredSchema.empty()) {
            if (!gApi->select_schema(session, gPreferredSchema.c_str())) {
                gApi->select_schema(session, "rime_ice");
            }
        } else if (!gApi->select_schema(session, "rime_ice")) {
            gApi->select_schema(session, "mi_pinyin");
        }
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
    if (gApi->is_maintenance_mode && gApi->is_maintenance_mode()) {
        if (gApi->join_maintenance_thread) {
            gApi->join_maintenance_thread();
        }
        if (gApi->is_maintenance_mode && gApi->is_maintenance_mode()) {
            return 0;
        }
    }
    if (*input == '\0') {
        if (gApi->clear_composition) {
            gApi->clear_composition(id);
        }
        return 0;
    }
    bool fed = false;
    if (gApi->simulate_key_sequence) {
        if (gApi->clear_composition) {
            gApi->clear_composition(id);
        }
        fed = gApi->simulate_key_sequence(id, input) == True;
    }
    if (!fed && gApi->process_key) {
        if (gApi->clear_composition) {
            gApi->clear_composition(id);
        }
        fed = true;
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(input);
             *p != '\0';
             ++p) {
            if (!gApi->process_key(id, static_cast<int>(*p), 0)) {
                fed = false;
                break;
            }
        }
    }
    if (!fed) {
        if (gApi->clear_composition) {
            gApi->clear_composition(id);
        }
        if (!gApi->set_input || !gApi->set_input(id, input)) {
            return 0;
        }
    }
    size_t remaining = out_size;
    char *cursor = out_buffer;
    int written = 0;

    auto appendCandidate = [&](const char *cand) -> bool {
        if (!cand || !*cand) {
            return true;
        }
        if (written > 0) {
            if (remaining <= 1) {
                return false;
            }
            *cursor++ = '\n';
            remaining--;
        }
        size_t len = std::strlen(cand);
        if (len >= remaining) {
            len = remaining - 1;
        }
        if (len == 0) {
            return false;
        }
        std::memcpy(cursor, cand, len);
        cursor += len;
        remaining -= len;
        *cursor = '\0';
        written++;
        return written < max_candidates;
    };

    if (gApi->candidate_list_begin && gApi->candidate_list_next &&
        gApi->candidate_list_end) {
        RimeCandidateListIterator iter;
        if (gApi->candidate_list_begin(id, &iter)) {
            while (written < max_candidates && gApi->candidate_list_next(&iter)) {
                if (!appendCandidate(iter.candidate.text)) {
                    break;
                }
            }
            gApi->candidate_list_end(&iter);
        }
    }

    if (written == 0) {
        RIME_STRUCT(RimeContext, ctx);
        if (!gApi->get_context || !gApi->get_context(id, &ctx)) {
            return 0;
        }
        const int total = std::min(ctx.menu.num_candidates, max_candidates);
        for (int i = 0; i < total; ++i) {
            if (!appendCandidate(ctx.menu.candidates[i].text)) {
                break;
            }
        }
        if (gApi->free_context) {
            gApi->free_context(&ctx);
        }
    }
    return written;
}

MI_IME_EXPORT int MiImeGetPreedit(void *session, char *out_buffer, size_t out_size) {
    if (!gApi || !session || !out_buffer || out_size == 0) {
        return 0;
    }
    out_buffer[0] = '\0';
    const RimeSessionId id = reinterpret_cast<RimeSessionId>(session);
    RIME_STRUCT(RimeContext, ctx);
    if (!gApi->get_context || !gApi->get_context(id, &ctx)) {
        return 0;
    }
    const char *preedit = ctx.composition.preedit;
    if (!preedit || !*preedit) {
        if (gApi->free_context) {
            gApi->free_context(&ctx);
        }
        return 0;
    }
    size_t len = std::strlen(preedit);
    if (len >= out_size) {
        len = out_size - 1;
    }
    if (len > 0) {
        std::memcpy(out_buffer, preedit, len);
        out_buffer[len] = '\0';
    }
    if (gApi->free_context) {
        gApi->free_context(&ctx);
    }
    return static_cast<int>(len);
}

MI_IME_EXPORT bool MiImeCommitCandidate(void *session, int index) {
    if (!gApi || !session || index < 0) {
        return false;
    }
    const RimeSessionId id = reinterpret_cast<RimeSessionId>(session);
    Bool selected = False;
    if (gApi->select_candidate_on_current_page) {
        selected = gApi->select_candidate_on_current_page(id, static_cast<size_t>(index));
    } else if (gApi->select_candidate) {
        selected = gApi->select_candidate(id, static_cast<size_t>(index));
    }
    if (!selected) {
        return false;
    }
    Bool committed = False;
    if (gApi->commit_composition) {
        committed = gApi->commit_composition(id);
    }
    if (gApi->clear_composition) {
        gApi->clear_composition(id);
    }
    return committed == True;
}

MI_IME_EXPORT void MiImeClearComposition(void *session) {
    if (!gApi || !session || !gApi->clear_composition) {
        return;
    }
    const RimeSessionId id = reinterpret_cast<RimeSessionId>(session);
    gApi->clear_composition(id);
}
}  // extern "C"
