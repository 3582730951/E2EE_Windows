#include "include/client_core.h"
#include "include/endpoint_hardening.h"

namespace {
struct EarlyEndpointHardening {
    EarlyEndpointHardening() noexcept {
        mi::client::security::StartEndpointHardening();
    }
};

EarlyEndpointHardening gEarlyEndpointHardening;
}  // namespace

int main() {
    mi::client::ClientCore client;
    if (!client.Init("config.ini")) {
        return 1;
    }
    if (!client.Login("u", "p")) {
        return 1;
    }
    if (!client.JoinGroup("g1")) {
        return 1;
    }
    if (!client.SendGroupMessage("g1", 1)) {
        return 1;
    }
    return 0;
}
