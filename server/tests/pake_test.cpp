#include <cassert>
#include <string>

#include "pake.h"

using mi::server::DerivedKeys;
using mi::server::DeriveKeysFromCredentials;
using mi::server::DeriveKeysFromPake;

int main() {
  DerivedKeys a{};
  DerivedKeys b{};
  std::string err;

  bool ok = DeriveKeysFromCredentials("u", "p", a, err);
  if (!ok) {
    return 1;
  }
  ok = DeriveKeysFromPake("u:p", b, err);
  if (!ok) {
    return 1;
  }

  if (a.root_key != b.root_key || a.header_key != b.header_key ||
      a.kcp_key != b.kcp_key || a.ratchet_root != b.ratchet_root) {
    return 1;
  }

  err.clear();
  DerivedKeys c{};
  ok = DeriveKeysFromCredentials("", "", c, err);
  if (ok) {
    return 1;
  }
  return 0;
}
