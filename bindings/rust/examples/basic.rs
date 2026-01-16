use mi_e2ee_client::Client;

fn main() {
    Client::check_abi().expect("mi_e2ee sdk abi mismatch");
    let v = Client::version();
    println!("mi_e2ee client sdk {}.{}.{} (abi {})", v.major, v.minor, v.patch, v.abi);
    println!("capabilities: 0x{:08x}", Client::capabilities());
}
