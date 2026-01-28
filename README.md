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
- 传输：TLS（Schannel/OpenSSL）+ CA/指纹/hybrid 校验 + 降级检测
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

元数据保护威胁模型（边界说明）：
- E2EE 攻击模型：内容端到端加密，但服务端运行态仍可见路由/在线关系；落库状态使用独立元数据密钥加密，降低离线泄露。
- 逆向/调试：IDA/OllyDbg 等动态调试与 Hook/注入会放大攻击面；端点硬化/反调试仅提高成本，无法对抗完全控制主机。
- Root/提取：Android root/Linux root 可读取落地密钥或内存；SecureStore/密钥保护可缓解但不保证绝对安全。

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
   - `tls_verify_mode=pin|ca|hybrid`（pin 需 `pinned_fingerprint` 或信任库条目；hybrid 可选指纹，设置后强校验）
   - `tls_ca_bundle_path=`（ca/hybrid 可选；为空使用系统/默认 CA）
   - `require_pinned_fingerprint=1` + `pinned_fingerprint=...`（legacy / pin；hybrid 可选 pin）
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
Linux 可选依赖：安装 `libavcodec` / `libavutil` / `libswscale` 可启用 H264 编解码，否则回退 RAW。
macOS 使用 VideoToolbox/AVFoundation H264 编解码（系统自带，无需额外依赖）。

### 关闭 UI（仅核心库）
```powershell
cmake -S client -B build/client -DMI_E2EE_BUILD_UI=OFF
```

### Android（JNI/Compose）
1. 构建 OpenSSL（建议）：
   - `tools/build_android_openssl.sh --ndk <NDK路径> --out <输出目录>`
   - 默认输出可用 `build/openssl/android`（会被自动识别）
2. 传入 OpenSSL 根目录：
   - 环境变量：`MI_E2EE_ANDROID_OPENSSL_ROOT`
   - 或 Gradle 属性：`-PmiE2eeAndroidOpenSslRoot=<路径>`
3. 运行 JNI 冒烟测试（需设备/模拟器）：
   - `cd android; .\gradlew.bat :app:connectedAndroidTest`
4. 如需临时允许 TLS stub（仅调试/测试）：
   - `MI_E2EE_ANDROID_ALLOW_TLS_STUB=1`

## 性能评估基准环境
- PC：Intel i3 8 代 CPU，8GB 内存环境
- Android：骁龙 888，4GB 内存环境

## 配置要点（摘要）
服务端 `config.ini`：
- `mode=0/1`（mysql/demo）
- `tls_enable=1` + `require_tls=1`
- `tls_cert=mi_e2ee_server.pfx`
- `kt_signing_key=kt_signing_key.bin`
- `offline_dir=offline_store`
- `state_backend=file|mysql|sql`（MySQL 兼容库，如 MariaDB/TiDB；状态表为单表 blob，便于迁移至兼容 SQL/半结构化 SQL 数据库）
- `metadata_protection` / `metadata_key_path`（元数据密钥保护与落地路径）

客户端 `client_config.ini`：
- `use_tls=1` + `require_tls=1`
- `tls_verify_mode=pin|ca|hybrid`（pin 需 `pinned_fingerprint` 或信任库条目；hybrid 可选指纹）
- `tls_ca_bundle_path=`（ca/hybrid 可选；为空使用系统/默认 CA）
- `require_pinned_fingerprint=1` + `pinned_fingerprint=...`（legacy / pin；hybrid 可选 pin）
- `[kt] require_signature=1` + `root_pubkey_path=kt_root_pub.bin`
- `[traffic] cover_traffic_enabled=1`
- `[device_sync] ratchet_enable=1` + `ratchet_max_skip=2048`

运行时环境变量：
- `MI_E2EE_HARDENING=off|low|medium|high`（或 `MI_E2EE_HARDENING_LEVEL`，默认 high）
- 硬化等级行为：low=仅基础进程缓解；medium=增加调试器检测；high=调试器+硬件断点检测+`.text`完整性扫描
- `MI_E2EE_SECCOMP=1`（Linux + libseccomp 可选；启用基础 seccomp denylist）
- `MI_E2EE_MAC_REQUIRE_SIGNATURE=1`（macOS：强制代码签名有效）
- `MI_E2EE_MAC_REQUIRE_SANDBOX=1`（macOS：强制 app sandbox entitlement）

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
