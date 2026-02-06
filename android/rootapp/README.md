Android RootAuth App

- Standalone trusted Root Auth app for 5-second rotating auth codes (SHA-256).
- Generates an Ed25519 keypair and stores it with Android Keystore
  (EncryptedSharedPreferences, best-effort StrongBox).
- 6-digit auth code, refreshed every 5 seconds.
- Scan login QR codes and show device info.
- Auth string (code:signature) for device-bound authorization.

Usage
1) Open this app and copy the public key (64-hex).
2) On a trusted device, initialize RootAuth on the server with that public key.
3) Use this app to generate auth strings to authorize new device logins.

Build
- From the android directory: ./gradlew :rootapp:assembleDebug
