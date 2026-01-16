import os

try:
    from cffi import FFI
except ImportError as exc:
    raise ImportError("cffi is required for mi_e2ee_client_cffi") from exc

MI_E2EE_SDK_ABI_VERSION = 1

_CDEF_PREFIX = """
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed int int32_t;
"""


def _load_cdef():
    base_dir = os.path.dirname(__file__)
    header_path = os.path.abspath(
        os.path.join(base_dir, "..", "..", "sdk", "c_api_client.h")
    )
    with open(header_path, "r", encoding="utf-8") as f:
        lines = []
        stop_after = False
        for line in f:
            if stop_after:
                continue
            stripped = line.lstrip()
            if stripped.startswith("#"):
                continue
            if 'extern "C"' in line:
                continue
            if stripped.strip() == "}":
                continue
            lines.append(line.rstrip("\n"))
            if "mi_client_free" in line:
                stop_after = True
    while lines and not lines[-1].strip():
        lines.pop()
    if lines and lines[-1].strip() == "}":
        lines.pop()
    text = "\n".join(lines)
    replacements = {
        "std::uint8_t": "uint8_t",
        "std::uint16_t": "uint16_t",
        "std::uint32_t": "uint32_t",
        "std::uint64_t": "uint64_t",
        "std::int32_t": "int32_t",
        "std::size_t": "size_t",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    text = text.replace("MI_E2EE_SDK_API", "")
    return _CDEF_PREFIX + "\n" + text


def _load_library(ffi):
    env = os.getenv("MI_E2EE_CLIENT_DLL")
    if env:
        return ffi.dlopen(env)
    candidates = [
        "mi_e2ee_client_sdk.dll",
        "libmi_e2ee_client_sdk.so",
        "libmi_e2ee_client_sdk.dylib",
        "mi_e2ee_client_sdk",
        "mi_e2ee_client_core.dll",
        "libmi_e2ee_client_core.so",
        "libmi_e2ee_client_core.dylib",
        "mi_e2ee_client_core",
    ]
    for name in candidates:
        try:
            return ffi.dlopen(name)
        except OSError:
            continue
    raise OSError("mi_e2ee_client_sdk shared library not found")


ffi = FFI()
ffi.cdef(_load_cdef())
lib = _load_library(ffi)
_abi_checked = False


def _cstr(ptr):
    if ptr == ffi.NULL:
        return ""
    return ffi.string(ptr).decode("utf-8")


def sdk_version():
    out = ffi.new("mi_sdk_version*")
    lib.mi_client_get_version(out)
    return out[0]


def capabilities():
    return int(lib.mi_client_get_capabilities())


def _check_abi():
    global _abi_checked
    if _abi_checked:
        return
    ver = sdk_version()
    if ver.abi != MI_E2EE_SDK_ABI_VERSION:
        raise RuntimeError(
            f"mi_e2ee sdk abi mismatch: {ver.abi} != {MI_E2EE_SDK_ABI_VERSION}"
        )
    _abi_checked = True


def check_abi():
    _check_abi()


def client_create(config_path=None):
    _check_abi()
    cfg = ffi.NULL
    if config_path:
        cfg = ffi.new("char[]", config_path.encode("utf-8"))
    handle = lib.mi_client_create(cfg)
    if handle == ffi.NULL:
        raise RuntimeError(_cstr(lib.mi_client_last_create_error()))
    return handle


def client_destroy(handle):
    if handle and handle != ffi.NULL:
        lib.mi_client_destroy(handle)


class MiClient:
    def __init__(self, config_path=None):
        self._handle = client_create(config_path)

    def close(self):
        if self._handle != ffi.NULL:
            client_destroy(self._handle)
            self._handle = ffi.NULL

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
