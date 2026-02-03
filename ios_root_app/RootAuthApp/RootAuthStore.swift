import Foundation
import CryptoKit
import Security

final class RootAuthStore: ObservableObject {
    @Published var secretHex: String = ""
    @Published var currentCode: String = "------"
    @Published var secondsRemaining: Int = 0

    private let keychainService = "mi.e2ee.rootauth"
    private let keychainAccount = "root_auth_secret"
    private var timer: Timer?

    init() {
        if let stored = loadSecret() {
            secretHex = stored
        }
        startTimer()
    }

    deinit {
        timer?.invalidate()
    }

    func startTimer() {
        timer?.invalidate()
        updateCode()
        timer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.updateCode()
        }
        RunLoop.main.add(timer!, forMode: .common)
    }

    func setSecret(hex: String) -> Bool {
        let cleaned = hex.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard cleaned.range(of: "^[0-9a-f]{64}$", options: .regularExpression) != nil else {
            return false
        }
        secretHex = cleaned
        saveSecret(cleaned)
        updateCode()
        return true
    }

    func clearSecret() {
        secretHex = ""
        currentCode = "------"
        secondsRemaining = 0
        deleteSecret()
    }

    func updateCode() {
        let now = Date()
        let step: TimeInterval = 5
        let counter = Int(now.timeIntervalSince1970 / step)
        let remaining = Int(step - (now.timeIntervalSince1970.truncatingRemainder(dividingBy: step)))
        secondsRemaining = remaining

        guard let secretData = hexToData(secretHex), secretData.count == 32 else {
            currentCode = "------"
            return
        }
        currentCode = totp(secret: secretData, counter: counter, digits: 6)
    }

    private func totp(secret: Data, counter: Int, digits: Int) -> String {
        var msg = [UInt8](repeating: 0, count: 8)
        var value = UInt64(counter)
        for i in stride(from: 7, through: 0, by: -1) {
            msg[i] = UInt8(value & 0xff)
            value >>= 8
        }
        let key = SymmetricKey(data: secret)
        let hmac = HMAC<SHA256>.authenticationCode(for: msg, using: key)
        let digest = Array(hmac)
        let offset = Int(digest.last! & 0x0f)
        let binary = ((Int(digest[offset]) & 0x7f) << 24) |
            ((Int(digest[offset + 1]) & 0xff) << 16) |
            ((Int(digest[offset + 2]) & 0xff) << 8) |
            (Int(digest[offset + 3]) & 0xff)
        let mod = digits == 8 ? 100_000_000 : 1_000_000
        let code = binary % mod
        return String(format: "%0*d", digits, code)
    }

    private func hexToData(_ hex: String) -> Data? {
        let cleaned = hex.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        guard cleaned.count % 2 == 0 else { return nil }
        var data = Data(capacity: cleaned.count / 2)
        var index = cleaned.startIndex
        while index < cleaned.endIndex {
            let next = cleaned.index(index, offsetBy: 2)
            let byteString = cleaned[index..<next]
            guard let byte = UInt8(byteString, radix: 16) else { return nil }
            data.append(byte)
            index = next
        }
        return data
    }

    private func saveSecret(_ secret: String) {
        let data = Data(secret.utf8)
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: keychainAccount
        ]
        SecItemDelete(query as CFDictionary)
        let add: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: keychainAccount,
            kSecValueData as String: data
        ]
        SecItemAdd(add as CFDictionary, nil)
    }

    private func loadSecret() -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: keychainAccount,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)
        guard status == errSecSuccess, let data = item as? Data else { return nil }
        return String(data: data, encoding: .utf8)
    }

    private func deleteSecret() {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: keychainAccount
        ]
        SecItemDelete(query as CFDictionary)
    }
}
