import Foundation
import CryptoKit
import Security

final class RootAuthStore: ObservableObject {
    @Published var publicKeyHex: String = ""
    @Published var currentCode: String = "------"
    @Published var secondsRemaining: Int = 0

    private let keychainService = "mi.e2ee.rootauth"
    private let keychainAccountPlain = "root_auth_sk_plain"
    private let keychainAccountCipher = "root_auth_sk_cipher"
    private let keychainKeyTag = "mi.e2ee.rootauth.key"
    private var timer: Timer?
    private var signingKey: Curve25519.Signing.PrivateKey?

    init() {
        signingKey = loadOrCreateSigningKey()
        refreshPublicKey()
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

    func regenerateKey() {
        signingKey = Curve25519.Signing.PrivateKey()
        saveSigningKey(signingKey)
        refreshPublicKey()
        updateCode()
    }

    func updateCode() {
        let now = Date()
        let step: TimeInterval = 5
        let counter = UInt64(now.timeIntervalSince1970 / step)
        let remaining = Int(step - (now.timeIntervalSince1970.truncatingRemainder(dividingBy: step)))
        secondsRemaining = remaining

        guard let pub = signingKey?.publicKey.rawRepresentation else {
            currentCode = "------"
            return
        }
        currentCode = deriveCode(pubkey: pub, counter: counter, digits: 6)
    }

    func authProof(deviceId: String, context: String, stepSec: TimeInterval = 5) -> String? {
        guard !deviceId.isEmpty, let key = signingKey else {
            return nil
        }
        let counter = UInt64(Date().timeIntervalSince1970 / stepSec)
        return signProof(key: key, deviceId: deviceId, context: context, counter: counter)
    }

    func authString(deviceId: String, context: String, stepSec: TimeInterval = 5) -> String? {
        guard !deviceId.isEmpty, let key = signingKey else {
            return nil
        }
        let counter = UInt64(Date().timeIntervalSince1970 / stepSec)
        let pub = key.publicKey.rawRepresentation
        let code = deriveCode(pubkey: pub, counter: counter, digits: 6)
        guard let proof = signProof(key: key, deviceId: deviceId, context: context,
                                    counter: counter) else {
            return nil
        }
        return "\(code):\(proof)"
    }

    private func refreshPublicKey() {
        if let pub = signingKey?.publicKey.rawRepresentation {
            publicKeyHex = hexString(pub)
        } else {
            publicKeyHex = ""
        }
    }

    private func deriveCode(pubkey: Data, counter: UInt64, digits: Int) -> String {
        var msg = Data()
        msg.append(Data("mi_e2ee_root_code_v1".utf8))
        msg.append(0)
        msg.append(pubkey)
        msg.append(0)
        var ctr = counter.bigEndian
        withUnsafeBytes(of: &ctr) { msg.append(contentsOf: $0) }
        let digest = SHA256.hash(data: msg)
        let bytes = Array(digest)
        let bin = (UInt32(bytes[0]) << 24) |
            (UInt32(bytes[1]) << 16) |
            (UInt32(bytes[2]) << 8) |
            UInt32(bytes[3])
        let mod: UInt32 = digits == 8 ? 100_000_000 : 1_000_000
        let code = bin % mod
        return String(format: "%0*d", digits, code)
    }

    private func buildProofMessage(deviceId: String, context: String, counter: UInt64) -> Data {
        var msg = Data()
        msg.append(Data("mi_e2ee_root_proof_v2".utf8))
        msg.append(0)
        msg.append(Data(deviceId.utf8))
        msg.append(0)
        msg.append(Data(context.utf8))
        msg.append(0)
        var ctr = counter.bigEndian
        withUnsafeBytes(of: &ctr) { msg.append(contentsOf: $0) }
        return msg
    }

    private func signProof(key: Curve25519.Signing.PrivateKey,
                           deviceId: String,
                           context: String,
                           counter: UInt64) -> String? {
        let msg = buildProofMessage(deviceId: deviceId, context: context, counter: counter)
        guard let signature = try? key.signature(for: msg) else {
            return nil
        }
        return hexString(signature)
    }

    private func hexString(_ data: Data) -> String {
        data.map { String(format: "%02x", $0) }.joined()
    }

    private func saveSigningKey(_ key: Curve25519.Signing.PrivateKey?) {
        guard let key = key else {
            deleteKey()
            return
        }
        let raw = key.rawRepresentation
        deleteKey()
        if let cipher = encryptData(raw) {
            storeData(cipher, account: keychainAccountCipher)
            return
        }
        storeData(raw, account: keychainAccountPlain)
    }

    private func loadOrCreateSigningKey() -> Curve25519.Signing.PrivateKey? {
        if let key = loadSigningKey() {
            return key
        }
        let key = Curve25519.Signing.PrivateKey()
        saveSigningKey(key)
        return key
    }

    private func loadSigningKey() -> Curve25519.Signing.PrivateKey? {
        if let cipher = loadData(account: keychainAccountCipher) {
            if let decoded = decryptData(cipher) {
                return try? Curve25519.Signing.PrivateKey(rawRepresentation: decoded)
            }
        }
        if let plain = loadData(account: keychainAccountPlain) {
            if let key = try? Curve25519.Signing.PrivateKey(rawRepresentation: plain) {
                if let cipher = encryptData(plain) {
                    storeData(cipher, account: keychainAccountCipher)
                    deleteData(account: keychainAccountPlain)
                }
                return key
            }
        }
        return nil
    }

    private func deleteKey() {
        deleteData(account: keychainAccountPlain)
        deleteData(account: keychainAccountCipher)
    }

    private func storeData(_ data: Data, account: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: account
        ]
        SecItemDelete(query as CFDictionary)
        let add: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: account,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            kSecValueData as String: data
        ]
        SecItemAdd(add as CFDictionary, nil)
    }

    private func loadData(account: String) -> Data? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: account,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)
        guard status == errSecSuccess, let data = item as? Data else { return nil }
        return data
    }

    private func deleteData(account: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: account
        ]
        SecItemDelete(query as CFDictionary)
    }

    private func loadOrCreateWrapKey() -> SecKey? {
        guard let tag = keychainKeyTag.data(using: .utf8) else { return nil }
        let query: [String: Any] = [
            kSecClass as String: kSecClassKey,
            kSecAttrApplicationTag as String: tag,
            kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
            kSecReturnRef as String: true
        ]
        var item: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &item)
        if status == errSecSuccess, let item {
            return item as! SecKey
        }
        if status != errSecItemNotFound {
            return nil
        }
        var privateAttrs: [String: Any] = [
            kSecAttrIsPermanent as String: true,
            kSecAttrApplicationTag as String: tag
        ]
        if let access = SecAccessControlCreateWithFlags(
            nil,
            kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
            [.privateKeyUsage],
            nil
        ) {
            privateAttrs[kSecAttrAccessControl as String] = access
        } else {
            privateAttrs[kSecAttrAccessible as String] =
                kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        }
        var attributes: [String: Any] = [
            kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
            kSecAttrKeySizeInBits as String: 256,
            kSecPrivateKeyAttrs as String: privateAttrs
        ]
        var error: Unmanaged<CFError>?
        if #available(iOS 10.0, *) {
            attributes[kSecAttrTokenID as String] = kSecAttrTokenIDSecureEnclave
        }
        if let key = SecKeyCreateRandomKey(attributes as CFDictionary, &error) {
            return key
        }
        attributes.removeValue(forKey: kSecAttrTokenID as String)
        error = nil
        return SecKeyCreateRandomKey(attributes as CFDictionary, &error)
    }

    private func encryptData(_ data: Data) -> Data? {
        guard let key = loadOrCreateWrapKey(),
              let publicKey = SecKeyCopyPublicKey(key) else { return nil }
        let algorithm = SecKeyAlgorithm.eciesEncryptionCofactorX963SHA256AESGCM
        guard SecKeyIsAlgorithmSupported(publicKey, .encrypt, algorithm) else {
            return nil
        }
        var error: Unmanaged<CFError>?
        guard let cipher = SecKeyCreateEncryptedData(publicKey, algorithm,
                                                     data as CFData, &error) else {
            return nil
        }
        return cipher as Data
    }

    private func decryptData(_ data: Data) -> Data? {
        guard let key = loadOrCreateWrapKey() else { return nil }
        let algorithm = SecKeyAlgorithm.eciesEncryptionCofactorX963SHA256AESGCM
        guard SecKeyIsAlgorithmSupported(key, .decrypt, algorithm) else {
            return nil
        }
        var error: Unmanaged<CFError>?
        guard let plain = SecKeyCreateDecryptedData(key, algorithm,
                                                    data as CFData, &error) else {
            return nil
        }
        return plain as Data
    }
}
