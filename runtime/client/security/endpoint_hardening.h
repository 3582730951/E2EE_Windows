#ifndef MI_E2EE_ENDPOINT_HARDENING_H
#define MI_E2EE_ENDPOINT_HARDENING_H

namespace mi::client::security {

// Best-effort endpoint hardening for the local process.
// - No logging / no persistence.
// - On Windows: applies process mitigations and starts in-process self-check
//   threads (default degrade; fail-closed opt-in).
// - On macOS/Linux: best-effort trace/signature checks.
void StartEndpointHardening() noexcept;

}  // namespace mi::client::security

#endif  // MI_E2EE_ENDPOINT_HARDENING_H
