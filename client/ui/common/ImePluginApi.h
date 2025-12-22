// IME plugin API used for dynamic loading.
#pragma once

#include <cstddef>

#if defined(_WIN32)
#if defined(MI_IME_PLUGIN_BUILD)
#define MI_IME_EXPORT __declspec(dllexport)
#else
#define MI_IME_EXPORT
#endif
#else
#define MI_IME_EXPORT
#endif

constexpr int kMiImeApiVersion = 3;

extern "C" {
MI_IME_EXPORT int MiImeApiVersion();
MI_IME_EXPORT bool MiImeInitialize(const char *shared_dir, const char *user_dir);
MI_IME_EXPORT void MiImeShutdown();
MI_IME_EXPORT void *MiImeCreateSession();
MI_IME_EXPORT void MiImeDestroySession(void *session);
MI_IME_EXPORT int MiImeGetCandidates(void *session,
                                     const char *input,
                                     char *out_buffer,
                                     size_t out_size,
                                     int max_candidates);
MI_IME_EXPORT int MiImeGetPreedit(void *session,
                                  char *out_buffer,
                                  size_t out_size);
MI_IME_EXPORT bool MiImeCommitCandidate(void *session, int index);
MI_IME_EXPORT void MiImeClearComposition(void *session);
}
