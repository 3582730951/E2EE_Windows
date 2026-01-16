# MI_E2EE SDK bindings

This directory hosts Rust and Python bindings built on top of the stable C SDK.

Python (ctypes):
- Library selection: set `MI_E2EE_CLIENT_DLL` to the SDK shared library path.
- Example: `python bindings/python/example_basic.py`

Python (cffi):
- Library selection: set `MI_E2EE_CLIENT_DLL` to the SDK shared library path.
- Example: `python bindings/python/example_basic_cffi.py`

Rust:
- Library search: set `MI_E2EE_CLIENT_LIB_DIR` to the SDK library directory.
- Build: `cargo build --manifest-path bindings/rust/Cargo.toml --examples`
- Example: `cargo run --manifest-path bindings/rust/Cargo.toml --example basic`
- Bindgen (optional, requires libclang): `cargo build --manifest-path bindings/rust/Cargo.toml --features bindgen` (generated at `target/.../out/bindings.rs`, exposed as `mi_e2ee_client::generated`)
- Cbindgen (optional): `cbindgen --config bindings/rust/cbindgen.toml --crate mi_e2ee_client --output bindings/rust/mi_e2ee_client.h`

Headers:
- C SDK header: `sdk/c_api_client.h`
