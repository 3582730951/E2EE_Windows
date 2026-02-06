RootAuth iOS App

Purpose
- Standalone trusted Root Auth app for 5-second rotating auth codes (SHA-256).
- Scan login QR codes and show device info before authorizing.

Features
- 6-digit auth code, refreshed every 5 seconds.
- Scan login QR codes and prompt for approval.
- Auth string (code:signature) for device-bound authorization.
- Generates an Ed25519 keypair and exposes the public key (64-hex).
- Private key is stored device-only and encrypted with Secure Enclave key when available.

Build
1) Install XcodeGen (optional)
   - `brew install xcodegen`
2) From `ios_root_app` run:
   - `xcodegen`
3) Open the generated `RootAuthApp.xcodeproj` with Xcode and run.

Notes
- This repo does not bundle iOS networking or server bindings; the app focuses on code generation and QR verification.
- Server approval is completed by entering the auth string on the new device.
