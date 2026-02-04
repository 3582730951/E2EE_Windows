RootAuth iOS App

Purpose
- Standalone trusted Root Auth app for 5-second rotating auth codes (TOTP/HMAC-SHA256).
- Scan login QR codes and show device info before authorizing.

Features
- 6-digit auth code, refreshed every 5 seconds.
- Scan login QR codes and prompt for approval.
- Optional auth string (code:proof) for device-bound authorization.
- Import root secret via QR (mi_e2ee://root-auth?secret=...) or manual 64-hex entry.
- Secret is stored device-only and encrypted with Secure Enclave key when available.

Build
1) Install XcodeGen (optional)
   - `brew install xcodegen`
2) From `ios_root_app` run:
   - `xcodegen`
3) Open the generated `RootAuthApp.xcodeproj` with Xcode and run.

Notes
- This repo does not bundle iOS networking or server bindings; the app focuses on code generation and QR verification.
- Server approval is completed by entering the auth code on the new device.
