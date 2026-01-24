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
        return 0
    if not os.path.isfile(lib_path):
        return 1
    try:
        lib = ctypes.CDLL(lib_path)
    except OSError:
        return 1

    lib.mi_client_get_version.argtypes = [ctypes.POINTER(SdkVersion)]
    lib.mi_client_get_version.restype = None
    lib.mi_client_get_capabilities.argtypes = []
    lib.mi_client_get_capabilities.restype = ctypes.c_uint32

    version = SdkVersion()
    lib.mi_client_get_version(ctypes.byref(version))
    _ = lib.mi_client_get_capabilities()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
