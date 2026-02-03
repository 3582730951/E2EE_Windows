Android RootAuth App

- Standalone trusted Root Auth app for 5-second rotating auth codes (TOTP/HMAC-SHA256).
- Stores the root secret with Android Keystore (EncryptedSharedPreferences).
- Best-effort StrongBox-backed key when available.
- 6-digit auth code, refreshed every 5 seconds.
- Scan login/root-secret QR codes and show device info.

Usage
1) Get the root secret (64-hex) from RootAuth init on a trusted device.
2) Open this app, paste the secret, tap Save.
3) Use the rotating code to authorize new device logins.

Build
- From the android directory: ./gradlew :rootapp:assembleDebug
