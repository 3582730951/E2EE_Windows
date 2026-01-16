fn main() {
    if let Ok(dir) = std::env::var("MI_E2EE_CLIENT_LIB_DIR") {
        if !dir.is_empty() {
            println!("cargo:rustc-link-search=native={}", dir);
        }
    }
    let link_static = std::env::var("MI_E2EE_CLIENT_LINK_STATIC")
        .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
        .unwrap_or(false);
    if link_static {
        println!("cargo:rustc-link-lib=static=mi_e2ee_client_core");
    } else {
        println!("cargo:rustc-link-lib=dylib=mi_e2ee_client_sdk");
    }
    println!("cargo:rerun-if-env-changed=MI_E2EE_CLIENT_LIB_DIR");
    println!("cargo:rerun-if-env-changed=MI_E2EE_CLIENT_LINK_STATIC");

    #[cfg(feature = "bindgen")]
    generate_bindings();
}

#[cfg(feature = "bindgen")]
fn generate_bindings() {
    let manifest_dir =
        std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let header = manifest_dir.join("..").join("..").join("sdk").join("c_api_client.h");
    println!("cargo:rerun-if-changed={}", header.display());
    let out_dir = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());
    let out_path = out_dir.join("bindings.rs");
    let bindings = bindgen::Builder::default()
        .header(header.to_string_lossy())
        .allowlist_function("mi_.*")
        .allowlist_type("mi_.*")
        .allowlist_var("MI_.*")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("bindgen failed");
    bindings
        .write_to_file(out_path)
        .expect("bindgen write failed");
}
