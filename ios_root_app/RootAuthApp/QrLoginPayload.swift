import Foundation

struct QrLoginPayload {
    let qrId: String
    let secretHex: String
    let deviceId: String?

    static func parse(_ raw: String) -> QrLoginPayload? {
        if raw.lowercased().range(of: "^[0-9a-f]{64}$", options: .regularExpression) != nil {
            return QrLoginPayload(qrId: "", secretHex: raw.lowercased(), deviceId: nil)
        }
        guard let components = URLComponents(string: raw) else { return nil }
        if components.scheme == "mi_e2ee", components.host == "qr-login" {
            let id = components.queryItems?.first(where: { $0.name == "id" })?.value ?? ""
            let secret = components.queryItems?.first(where: { $0.name == "s" })?.value ?? ""
            let device = components.queryItems?.first(where: { $0.name == "d" })?.value
            guard !id.isEmpty, !secret.isEmpty else { return nil }
            return QrLoginPayload(qrId: id, secretHex: secret, deviceId: device)
        }
        if components.scheme == "mi_e2ee", components.host == "root-auth" {
            let secret = components.queryItems?.first(where: { $0.name == "secret" })?.value ?? ""
            guard !secret.isEmpty else { return nil }
            return QrLoginPayload(qrId: "", secretHex: secret, deviceId: nil)
        }
        return nil
    }
}
