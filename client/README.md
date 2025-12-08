# Client Core Usage (Demo)

1. 配置文件：复制 `client_config.example.ini` 为 `client_config.ini`，设置 `server_ip`/`server_port`；后端需有对应的 `config.ini` 与测试用户（demo 模式下 `test_user.txt`）。
2. 构建：
```
cd core/client
cmake -S . -B build
cmake --build build --config Release
```
3. 运行示例（使用 demo 用户 `u/p`，群 `g1`）：
```
./build/mi_e2ee_client   # Windows 对应 mi_e2ee_client.exe
```
4. 集成：链接静态库 `mi_e2ee_client_core`，使用 `mi::client::ClientCore` 进行 Init/Login/JoinGroup/SendGroupMessage；后续可在 UI 内直接调用。
