from mi_e2ee_client_cffi import sdk_version, capabilities, check_abi

check_abi()
ver = sdk_version()
print(f"mi_e2ee client sdk {ver.major}.{ver.minor}.{ver.patch} (abi {ver.abi})")
print(f"capabilities: 0x{capabilities():08x}")
