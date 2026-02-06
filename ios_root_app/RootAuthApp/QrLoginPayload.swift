import Foundation

struct QrLoginPayload {
    let qrId: String
    let secretHex: String
    let deviceId: String?
    let username: String?
    let host: String?
    let port: Int?
    let useTls: Bool
    let fingerprint: String?

    static func parse(_ raw: String) -> QrLoginPayload? {
        guard let components = URLComponents(string: raw) else { return nil }
        if components.scheme == "mi_e2ee", components.host == "qr-login" {
            let id = components.queryItems?.first(where: { $0.name == "id" })?.value ?? ""
            let secret = components.queryItems?.first(where: { $0.name == "s" })?.value ?? ""
            let device = components.queryItems?.first(where: { $0.name == "d" })?.value
            let username = components.queryItems?.first(where: { $0.name == "u" })?.value
            let host = components.queryItems?.first(where: { $0.name == "h" })?.value
            let port = components.queryItems?.first(where: { $0.name == "p" })?.value
                .flatMap { Int($0) }
            let tlsParam = components.queryItems?.first(where: { $0.name == "tls" })?.value
            let useTls = (tlsParam.flatMap { Int($0) } ?? 1) != 0
            let fingerprint = components.queryItems?.first(where: { $0.name == "fp" })?.value
            guard !id.isEmpty, !secret.isEmpty else { return nil }
            return QrLoginPayload(qrId: id, secretHex: secret, deviceId: device,
                                  username: username, host: host, port: port,
                                  useTls: useTls, fingerprint: fingerprint)
        }
        return nil
    }
}
