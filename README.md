# MI E2EE

[![License: PolyForm Noncommercial 1.0.0](https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-blue)](https://polyformproject.org/licenses/noncommercial/1.0.0)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20Android%20%7C%20iOS-445)

MI E2EE 是一个公开源码、非商业许可的端到端加密即时通信项目，包含服务端、Windows 客户端、Linux 客户端核心、Android 客户端、iOS RootAuth 示例和 Flutter 跨端客户端壳。项目目标是在不依赖服务端明文的前提下完成认证、会话协商、消息收发、群聊密钥轮换、离线文件保护、Key Transparency 校验和客户端 UI 明文保护。

> 许可提醒：本项目自有代码采用 PolyForm Noncommercial License 1.0.0，禁止商业用途。第三方开源库、词库、图标、模型和工具不被本项目重新授权，继续适用它们各自的许可证。

## 目录

- [项目简介](#项目简介)
- [项目结构](#项目结构)
- [架构](#架构)
- [如何配置](#如何配置)
- [许可证](#许可证)
- [第三方组件](#第三方组件)
- [鸣谢](#鸣谢)

## 项目简介

MI E2EE 面向端到端加密通信、安全研究和非商业部署验证。服务端负责账户校验、连接网关、密文转发、离线消息和离线文件存储；客户端负责密钥派生、消息加密解密、信任状态校验和本地明文展示控制。

核心能力：

- 认证：OPAQUE / PAKE，服务端不保存明文口令。
- 会话：X25519 + 可选 ML-KEM + HKDF 派生。
- 消息：Double Ratchet + AEAD，群聊 Sender Key 与成员变更/阈值轮换。
- Key Transparency：STH 签名、一致性校验和 gossip 阈值告警。
- 传输：TLS（Windows Schannel，POSIX/OpenSSL）+ CA/pin/hybrid 校验 + 降级检测。
- 元数据保护：消息、心跳、文件分块桶化填充和 cover traffic。
- 离线文件：一次一密、密钥删除优先、下载后擦除路径。
- UI 明文门禁：调试器、硬件断点、IAT/inline hook、私有可执行页、ntdll stub、录屏/截图风险共同参与判定。
- 客户端：Qt 6 Windows UI、Android Compose UI、iOS RootAuth Swift 示例、Flutter shell、C/Python/Rust 绑定。

项目边界：

- 适合安全研究、非商业集成验证和端到端加密通信原型。
- 不保证防御终端被 root/admin 完全控制、驱动级 hook、内核级恶意软件、外部摄像或虚拟机外部抓屏。
- 生产部署应启用 TLS、证书 pin、Key Transparency 签名校验、敏感日志禁用和严格配置权限。

## 项目结构

```text
core/
├── server/          服务端、TCP/KCP 网关、转发、离线消息/文件、ops health
├── client/          客户端核心库、SDK、Qt Quick UI、Windows native hardening
├── runtime/         客户端运行时服务、消息、同步、存储、安全、媒体
├── platform/        Windows/POSIX/Android 平台抽象
├── shard/           共享安全类型、媒体帧、OPAQUE Rust bridge
├── android/         Android JNI、Compose UI
├── app_flutter/     Flutter 跨端客户端壳
├── ios_root_app/    iOS RootAuth simulator 示例
├── third_party/     直接 vendored 的第三方代码和 SBOM/lock
└── tools/           打包、隐私保护、IME、辅助工具
```

## 架构

```text
┌─────────────┐      密文/控制面      ┌─────────────┐
│  Client UI  │ ───────────────────▶ │   Server    │
│ Qt/Android/ │                      │ TCP/KCP/TLS │
│ iOS/Flutter │ ◀─────────────────── │ Gateway     │
└──────┬──────┘      密文/离线数据    └──────┬──────┘
       │                                      │
       ▼                                      ▼
┌─────────────┐                      ┌─────────────┐
│ Client Core │                      │ Auth/KT/    │
│ SDK/Runtime │                      │ Offline/File│
└──────┬──────┘                      └─────────────┘
       │
       ▼
┌─────────────┐
│ shard/      │
│ platform/   │
└─────────────┘
```

主要数据流：

1. 客户端通过 PAKE 完成认证，服务端不接触明文口令。
2. 客户端协商会话密钥，消息层使用 ratchet 派生并用 AEAD 加密。
3. 群聊通过 Sender Key 管理群消息密钥，成员变更或阈值触发轮换。
4. 服务端只负责密文转发、离线密文存储、文件密文存储和透明日志发布。
5. 客户端展示明文前执行本地环境门禁，风险状态下不释放 UI 明文。

## 如何配置

Demo 模式可用于本地验证。生产环境必须启用 TLS、证书 pin、Key Transparency 签名校验，禁用敏感日志，并限制配置文件权限。

### 服务端配置

```ini
[mode]
mode=1

[server]
list_port=7000
tls_enable=1
require_tls=1
tls_cert=mi_e2ee_server.pfx
offline_dir=offline_store
debug_log=0

[kt]
require_signature=1
kt_signing_key=kt_signing_key.bin
```

首次启动时，如果 `kt_signing_key.bin` 不存在，服务端会生成 KT 签名私钥和 `kt_root_pub.bin`。

### 客户端配置

```ini
[client]
server_ip=127.0.0.1
server_port=7000
use_tls=1
require_tls=1
tls_verify_mode=pin
require_pinned_fingerprint=1
pinned_fingerprint=<sha256-cert-fingerprint>

[kt]
require_signature=1
root_pubkey_path=kt_root_pub.bin
```

服务端关键项：

- `mode=0|1`：MySQL 或 demo file。
- `mysql_ip/mysql_port/mysql_database/mysql_username/mysql_password`：MySQL 模式必填。
- `tls_enable=1` 与 `require_tls=1`：生产建议强制。
- `ops_enable=0`：运维接口默认关闭；开启时必须设置高熵 `ops_token`。
- `offline_dir`：应只允许服务账号读写。
- `debug_log=0`：生产禁用。

客户端关键项：

- `tls_verify_mode=pin|ca|hybrid`。
- `trust_store=server_trust.ini`。
- `[device_sync] enabled=0|1`，linked 设备应使用独立同步密钥。
- `MI_E2EE_HARDENING=off|low|medium|high`，Release 默认 high。
- `MI_E2EE_SENSITIVE_MODE=1` 或 `MI_E2EE_NO_HISTORY=1` 可禁用历史落盘。

### Linux 可选依赖

本地 Linux 环境缺少以下可选依赖时，相关能力会降级或关闭：

- `libsecret-1-dev`：系统安全存储集成。
- `libseccomp-dev`：基础 seccomp denylist。
- FFmpeg development packages：H264/媒体编解码；缺失时回退 RAW 或禁用对应路径。

## 许可证

### 项目自有代码

除下方第三方组件和文件内另有声明外，本仓库自有代码使用 [PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0)，许可证全文见 `LICENSE`。

适用范围包括：

- `server/`、`client/`、`runtime/`、`platform/`、`shard/` 中的项目自研 C/C++/Rust bridge 代码。
- `android/` 中的项目自研 JNI、Kotlin、Compose UI 和测试代码，第三方资源除外。
- `app_flutter/` 中的项目自研 Dart/Flutter 代码，第三方字体/包除外。
- `ios_root_app/` 中的项目自研 Swift 示例代码。
- `tools/`、`.github/`、配置模板、打包脚本和测试脚本。

禁止商业用途，包括但不限于：

- 作为商业产品、SaaS、企业内部商业系统或付费交付项目的一部分使用。
- 将项目自有代码集成到闭源或商业发行包。
- 为客户部署、托管、改造、二次销售或提供收费支持。

商业使用需要作者或权利人单独书面授权。

### 第三方代码和资源

第三方组件不被本项目的 PolyForm Noncommercial 许可证重新授权。它们继续适用各自上游许可证。使用、分发或修改这些组件时，需要同时满足上游许可证和本项目自有代码许可证。

## 第三方组件

| 组件 | 用途/路径 | 上游许可证 | 已验证地址 |
| --- | --- | --- | --- |
| Monocypher | `third_party/monocypher`，基础密码学实现 | BSD-2-Clause OR CC0-1.0 | <https://monocypher.org/> |
| miniz | `third_party/miniz`，ZIP/deflate | public domain / Unlicense style notice | <https://github.com/richgel999/miniz> |
| KCP | `third_party/kcp`，KCP ARQ 协议 | MIT | <https://github.com/skywind3000/kcp> |
| QR Code generator | `third_party/qrcodegen`，KT/登录二维码工具 | MIT | <https://www.nayuki.io/page/qr-code-generator-library> |
| ed25519 | `third_party/ed25519`，Ed25519 C 实现 | Zlib | <https://github.com/orlp/ed25519> |
| PQClean | `third_party/pqclean_*`，ML-KEM/ML-DSA clean 实现 | CC0-1.0 AND MIT | <https://github.com/PQClean/PQClean> |
| OpenSSL | POSIX TLS、Android TLS 构建 | OpenSSL/Apache-2.0 family,按所用版本 | <https://www.openssl.org/> |
| Qt 6 | Windows Qt Quick UI | Qt distribution license,通常 LGPL/GPL/商业授权之一 | <https://www.qt.io/> |
| Flutter / Dart | `app_flutter/` 客户端壳 | Flutter/Dart SDK licenses | <https://flutter.dev/> |
| AndroidX / Jetpack Compose | `android/` UI 与测试依赖 | AndroidX/Google Maven artifact licenses | <https://developer.android.com/jetpack/androidx> |
| Kotlin | Android Gradle/Kotlin 代码 | Kotlin project licenses | <https://kotlinlang.org/> |
| ZXing | Android QR code support | Apache-2.0 | <https://github.com/zxing/zxing> |
| opaque-ke | `shard/opaque_pake` Rust OPAQUE crate | Apache-2.0 OR MIT | <https://github.com/facebook/opaque-ke>, <https://docs.rs/opaque-ke/4.1.0-pre.1/opaque_ke/> |
| Rust crates | `argon2`、`bincode`、`rand`、`serde`、`sha2` 等 | 见 `shard/opaque_pake/Cargo.lock` 和 crate metadata | <https://docs.rs/argon2>, <https://docs.rs/bincode>, <https://docs.rs/rand>, <https://docs.rs/serde>, <https://docs.rs/sha2> |
| librime | Windows IME bridge API | BSD-3-Clause | <https://github.com/rime/librime> |
| rime-luna-pinyin | Rime 拼音词库/配置 | LGPL-3.0 | <https://github.com/rime/rime-luna-pinyin> |
| rime-ice | 雾凇拼音词库/配置 | GPL-3.0-only | <https://github.com/iDvel/rime-ice> |
| English word lists | 英文输入词库生成源 | MIT/各源许可证 | <https://github.com/en-wl/wordlist>, <https://github.com/shewer/rime-english> |
| 3dicons | Android UI 图标资源 | CC0-1.0 | <https://3dicons.co/> |
| FFmpeg | 可选媒体运行时/Windows 打包下载 | 取决于所用构建，LGPL/GPL 组合 | <https://ffmpeg.org/>, <https://github.com/BtbN/FFmpeg-Builds> |
| Real-ESRGAN | 可选离线图片超清运行时 | BSD-3-Clause | <https://github.com/xinntao/Real-ESRGAN> |
| ncnn | Real-ESRGAN ncnn Vulkan 推理后端 | BSD-3-Clause | <https://github.com/Tencent/ncnn> |
| CMake | 构建系统 | BSD-3-Clause | <https://cmake.org/> |
| Rust | OPAQUE bridge 构建工具链 | MIT OR Apache-2.0 | <https://www.rust-lang.org/> |

更细的 vendored 文件清单见 `third_party/third_party.lock` 和 `third_party/third_party.sbom.json`。移动端、Flutter、Rust 的传递依赖以对应 lockfile 为准。

## 鸣谢

本项目的协议设计和工程实现参考或受益于以下公开工作：

- [RFC 9807 OPAQUE](https://www.rfc-editor.org/rfc/rfc9807)：OPAQUE aPAKE 协议。
- [Signal Double Ratchet specification](https://signal.org/docs/specifications/doubleratchet/)：异步消息前向/后向安全设计参考。
- [Signal private group messaging](https://signal.org/blog/private-groups/)：群组与 sender-key 类设计思路参考。
- [Google Key Transparency](https://github.com/google/keytransparency)：公开可审计密钥目录与 Merkle 透明日志思路参考。
- [PQClean](https://github.com/PQClean/PQClean)、[Monocypher](https://monocypher.org/)、[OpenSSL](https://www.openssl.org/) 和 Rust Crypto 生态为密码学实现提供基础。
- [Rime](https://github.com/rime/librime)、[rime-luna-pinyin](https://github.com/rime/rime-luna-pinyin)、[rime-ice](https://github.com/iDvel/rime-ice) 为中文输入体验提供词库和 IME 生态。
- [Real-ESRGAN](https://github.com/xinntao/Real-ESRGAN)、[ncnn](https://github.com/Tencent/ncnn)、[FFmpeg](https://ffmpeg.org/) 为可选媒体能力提供工程基础。
- [Qt](https://www.qt.io/)、[Android Developers](https://developer.android.com/)、[Apple iOS Developer](https://developer.apple.com/ios/) 和 [Flutter](https://flutter.dev/) 的平台文档与工具链支撑跨端 UI。

上述链接已在 2026-04-27 UTC 写入 README 前重新验证。
