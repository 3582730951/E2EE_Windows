use mi_e2ee_client::Client;

fn main() {
    Client::check_abi().expect("mi_e2ee sdk abi mismatch");
    let _ = Client::version();
    let _ = Client::capabilities();
}
