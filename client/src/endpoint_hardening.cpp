#include "endpoint_hardening.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

#include "monocypher.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#endif

namespace mi::client::security {

namespace {

std::atomic<bool> gStarted{false};

#ifdef _WIN32

using SetProcessMitigationPolicyFn = BOOL(WINAPI*)(int, PVOID, SIZE_T);

constexpr DWORD kNoRemoteImagesFlag = 0x1u;
constexpr DWORD kNoLowMandatoryLabelImagesFlag = 0x2u;
constexpr DWORD kPreferSystem32ImagesFlag = 0x4u;

constexpr int kProcessExtensionPointDisablePolicy = 6;
constexpr int kProcessImageLoadPolicy = 10;

struct ExtensionPointDisablePolicy {
    std::uint32_t flags{0};
};
static_assert(sizeof(ExtensionPointDisablePolicy) == sizeof(std::uint32_t));

struct ImageLoadPolicy {
    std::uint32_t flags{0};
};
static_assert(sizeof(ImageLoadPolicy) == sizeof(std::uint32_t));

struct TextRegion {
    const std::uint8_t* base{nullptr};
    std::size_t size{0};
};

void ApplyBestEffortMitigations() noexcept {
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    SetDllDirectoryW(L"");

    const auto kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) return;

    const auto setProcessMitigationPolicy =
        reinterpret_cast<SetProcessMitigationPolicyFn>(
            GetProcAddress(kernel32, "SetProcessMitigationPolicy"));
    if (!setProcessMitigationPolicy) return;

    ExtensionPointDisablePolicy ext{};
    ext.flags = 0x1u;  // DisableExtensionPoints
    setProcessMitigationPolicy(kProcessExtensionPointDisablePolicy, &ext,
                               sizeof(ext));

    ImageLoadPolicy img{};
    img.flags = kNoRemoteImagesFlag | kNoLowMandatoryLabelImagesFlag |
                kPreferSystem32ImagesFlag;
    setProcessMitigationPolicy(kProcessImageLoadPolicy, &img, sizeof(img));
}

bool GetMainModuleTextRegion(TextRegion& out) noexcept {
    const auto exe = GetModuleHandleW(nullptr);
    if (!exe) return false;
    const auto* base = reinterpret_cast<const std::uint8_t*>(exe);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        base + static_cast<std::size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    const std::uint16_t count = nt->FileHeader.NumberOfSections;
    for (std::uint16_t i = 0; i < count; ++i) {
        const auto& sec = sections[i];
        if (std::memcmp(sec.Name, ".text", 5) != 0) continue;
        out.base = base + static_cast<std::size_t>(sec.VirtualAddress);
        out.size = static_cast<std::size_t>(sec.Misc.VirtualSize);
        return out.base != nullptr && out.size != 0;
    }
    return false;
}

std::array<std::uint8_t, 32> HashText(const TextRegion& region) noexcept {
    std::array<std::uint8_t, 32> hash{};
    crypto_blake2b(hash.data(), hash.size(), region.base, region.size);
    return hash;
}

void TerminateFailClosed(std::uint32_t code) noexcept {
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code));
}

void ScanThreadMain(TextRegion region,
                    std::array<std::uint8_t, 32> baseline) noexcept {
    for (;;) {
        const auto now = HashText(region);
        if (now != baseline) {
            TerminateFailClosed(0xE2EE0001u);
        }
        Sleep(1000);
    }
}

void MonitorThreadMain() noexcept {
    for (;;) {
        ApplyBestEffortMitigations();
        Sleep(5000);
    }
}

void StartThreadsBestEffort() noexcept {
    TextRegion region{};
    if (!GetMainModuleTextRegion(region)) return;
    const auto baseline = HashText(region);

    try {
        std::thread(ScanThreadMain, region, baseline).detach();
        std::thread(MonitorThreadMain).detach();
    } catch (...) {
    }
}

#endif  // _WIN32

}  // namespace

void StartEndpointHardening() noexcept {
    if (gStarted.exchange(true)) return;

#ifdef _WIN32
    ApplyBestEffortMitigations();
    StartThreadsBestEffort();
#endif
}

}  // namespace mi::client::security
