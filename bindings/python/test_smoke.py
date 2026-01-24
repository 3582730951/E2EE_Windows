#!/usr/bin/env python3
import ctypes
import os
import sys


class SdkVersion(ctypes.Structure):
    _fields_ = [
        ("major", ctypes.c_uint32),
        ("minor", ctypes.c_uint32),
        ("patch", ctypes.c_uint32),
        ("abi", ctypes.c_uint32),
    ]


def main() -> int:
    lib_path = os.environ.get("MI_E2EE_CLIENT_DLL", "").strip()
    if not lib_path:
        print("MI_E2EE_CLIENT_DLL not set; skipping.")
        return 0
    if not os.path.isfile(lib_path):
        print(f"MI_E2EE_CLIENT_DLL not found: {lib_path}")
        return 1

    try:
        lib = ctypes.CDLL(lib_path)
    except OSError as exc:
        print(f"Failed to load SDK library: {exc}")
        return 1

    lib.mi_client_get_version.argtypes = [ctypes.POINTER(SdkVersion)]
    lib.mi_client_get_version.restype = None
    lib.mi_client_get_capabilities.argtypes = []
    lib.mi_client_get_capabilities.restype = ctypes.c_uint32

    version = SdkVersion()
    lib.mi_client_get_version(ctypes.byref(version))
    caps = lib.mi_client_get_capabilities()

    print(
        f"mi_client_get_version: {version.major}.{version.minor}.{version.patch} abi={version.abi}"
    )
    print(f"mi_client_get_capabilities: {caps}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
