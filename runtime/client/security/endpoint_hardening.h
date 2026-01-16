#ifndef MI_E2EE_ENDPOINT_HARDENING_H
#define MI_E2EE_ENDPOINT_HARDENING_H

namespace mi::client::security {

// Best-effort endpoint hardening for the local process.
// - No logging / no persistence.
// - On Windows: applies process mitigations and starts in-process self-check
//   threads (fail-closed on detected tampering).
// - On other platforms: currently no-op.
void StartEndpointHardening() noexcept;

}  // namespace mi::client::security

#endif  // MI_E2EE_ENDPOINT_HARDENING_H

