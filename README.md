# MI E2EE (Windows)

[![Build](https://github.com/3582730951/E2EE_Windows/actions/workflows/ci.yml/badge.svg)](https://github.com/3582730951/E2EE_Windows/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Platform](https://img.shields.io/badge/platform-Windows-0078D4)

端到端加密聊天栈（服务端+客户端），面向 Windows 环境，包含 PAKE/OPAQUE 认证、双重 Ratchet、群聊 Sender Key、Key Transparency、离线文件加密与 UI 客户端。

## 目录
- 特性
- 架构与目录
- 安全模型与威胁边界
- 快速开始（Demo）
- 构建与测试
- 配置要点
- CI
- 贡献与反馈
- License

## 特性
- 认证：OPAQUE / PAKE，无明文口令存储
- 会话：X25519 + ML-KEM（可混合）+ HKDF 派生
- 消息：Double Ratchet + AEAD，Sender Key 群聊
- Key Transparency：STH 签名与一致性校验、gossip 阈值告警
- 传输：TLS（Windows Schannel）+ 证书指纹 pin + 降级检测
- 元数据对抗：消息/心跳/文件分块桶化填充 + cover traffic
- 离线文件：一次一密 + 密钥删除优先 + 可选 secure-delete 插件
- 客户端：Qt 6 UI（默认开启），核心逻辑走 `mi_e2ee_client_core`
- 图片：可选离线 AI 超清（手动触发，不强制使用）

## 架构与目录
```
server/          服务端（TCP/KCP 网关、转发、离线/文件）
client/          客户端核心库与 UI
shard/           共享安全类型与加扰逻辑（含 C 接口）
third_party/     第三方依赖（hash lock + SBOM）
tools/           工具（third_party_audit 等）
```

## 安全模型与威胁边界
已覆盖（显著提高攻击成本）：
- 旁路窃听、MITM、中间人证书替换（TLS + pin + 降级检测）
- 服务器被动窥探（内容端到端加密，服务端不解密）
- 群密钥滥用（Sender Key 轮换+分发 ACK）
- KT 分叉/回滚（STH 签名 + gossip 告警）

不保证完全防御：
- 终端被入侵（恶意软件、内存注入、键盘记录）
- 物理取证/系统级后门
- 高级流量关联与侧信道（仅降低可识别性）

## 快速开始（Demo）
> Demo 模式用于本地测试；生产环境请启用 TLS + 预置 pin + KT 签名校验。

### 1) 准备服务端
1. 复制 `server/config.example.ini` 到 `config.ini`。
2. Demo 模式：`[mode] mode=1`（MySQL 用 `mode=0`）。
3. TLS（建议保持开启）：
   - `tls_enable=1`
   - `require_tls=1`
   - `tls_cert=mi_e2ee_server.pfx`（Windows 上若不存在会自动生成自签 PFX）
4. Key Transparency 签名密钥（必需）：
   - `kt_signing_key=kt_signing_key.bin`
   - 该文件为 ML-DSA65 私钥（4032 字节）。若文件不存在，服务端首次启动会自动生成，并在同目录写入 `kt_root_pub.bin`。
5. 启动：运行 `mi_e2ee_server.exe`（保持窗口开启）。

### 2) 准备客户端
1. 复制 `client/client_config.example.ini` 到 `client_config.ini`。
2. 必填项：
   - `server_ip` / `server_port`
   - `use_tls=1` + `require_tls=1`
   - `require_pinned_fingerprint=1` + `pinned_fingerprint=...`
   - `[kt] require_signature=1` + `root_pubkey_path=kt_root_pub.bin`
3. 运行 `mi_e2ee.exe`，用 `test_user.txt` 中的账号登录。

### TLS 指纹获取（pinned_fingerprint）
PowerShell 示例（输出 64 位 hex）：
```powershell
$cert = Get-PfxCertificate -FilePath .\mi_e2ee_server.pfx
$der = $cert.Export('Cert')
$hash = [System.BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash($der)).Replace("-", "").ToLower()
$hash
```

### Key Transparency 密钥生成（kt_signing_key / kt_root_pub）
- 服务端首次启动会自动生成 `kt_signing_key.bin` 与 `kt_root_pub.bin`（同目录）。
- 手工生成或轮换（可覆盖已有文件）：
  - `mi_e2ee_kt_keygen --out-dir .`
  - 或 `mi_e2ee_kt_keygen --sk <path> --pk <path> --force`
- 客户端 `root_pubkey_path` 指向公钥文件（强制校验）。

## 构建与测试
### 服务端
```powershell
cmake -S server -B build/server
cmake --build build/server --config Release
ctest --output-on-failure --test-dir build/server
```

### 客户端（UI 默认开启）
```powershell
cmake -S client -B build/client
cmake --build build/client --config Release
ctest --output-on-failure --test-dir build/client
```

### 关闭 UI（仅核心库）
```powershell
cmake -S client -B build/client -DMI_E2EE_BUILD_UI=OFF
```

## 配置要点（摘要）
服务端 `config.ini`：
- `mode=0/1`（mysql/demo）
- `tls_enable=1` + `require_tls=1`
- `tls_cert=mi_e2ee_server.pfx`
- `kt_signing_key=kt_signing_key.bin`
- `offline_dir=offline_store`

客户端 `client_config.ini`：
- `use_tls=1` + `require_tls=1`
- `require_pinned_fingerprint=1` + `pinned_fingerprint=...`
- `[kt] require_signature=1` + `root_pubkey_path=kt_root_pub.bin`
- `[traffic] cover_traffic_enabled=1`

## CI
GitHub Actions：`.github/workflows/ci.yml`
- Windows 构建默认生成自签 TLS 证书并执行 Debug/Release 测试。

## 贡献与反馈
- 提交遵循 Conventional Commits
- PR 需包含变更摘要与测试结果

## 致谢
- 离线图片超清基于 Real-ESRGAN（ncnn 预训练模型），感谢 xinntao 与社区贡献。

## License
本项目使用 PolyForm Noncommercial License 1.0.0（见 `LICENSE`）。
允许个人非商业使用、拉取、修改、创建分支；商业使用需作者书面许可。
