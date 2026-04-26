import Foundation
import Security
import UIKit
import UniformTypeIdentifiers

enum SecureClipboard {
    private struct Entry {
        let plaintext: String
        let expiresAt: Date
    }

    private static let tokenPrefix = "zerox://clip/"
    private static let ttlSeconds: TimeInterval = 60
    private static let lock = NSLock()
    private static var entries: [String: Entry] = [:]

    static func copyProtectedText(_ plaintext: String) {
        purgeExpiredEntries()
        let token = tokenPrefix + randomToken()
        let expiresAt = Date().addingTimeInterval(ttlSeconds)
        lock.lock()
        entries[token] = Entry(plaintext: plaintext, expiresAt: expiresAt)
        lock.unlock()
        UIPasteboard.general.setItems(
            [[UTType.plainText.identifier: token]],
            options: [
                .localOnly: true,
                .expirationDate: expiresAt
            ]
        )
    }

    static func resolveProtectedText(_ value: String) -> String? {
        purgeExpiredEntries()
        guard value.hasPrefix(tokenPrefix) else {
            return nil
        }
        lock.lock()
        defer { lock.unlock() }
        guard let entry = entries[value], entry.expiresAt > Date() else {
            entries.removeValue(forKey: value)
            return nil
        }
        return entry.plaintext
    }

    static func clear() {
        lock.lock()
        entries.removeAll(keepingCapacity: false)
        lock.unlock()
        UIPasteboard.general.setItems([], options: [.localOnly: true])
    }

    private static func purgeExpiredEntries() {
        let now = Date()
        lock.lock()
        entries = entries.filter { $0.value.expiresAt > now }
        lock.unlock()
    }

    private static func randomToken() -> String {
        var bytes = [UInt8](repeating: 0, count: 32)
        let status = SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes)
        if status != errSecSuccess {
            return UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased()
        }
        return bytes.map { String(format: "%02x", $0) }.joined()
    }
}
