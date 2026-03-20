import Foundation
import Network
import CryptoKit
import Security

struct QrLoginApproveResult {
    let success: Bool
    let error: String?
}

private struct RootAuthNetworkReceiveError: Error {
    let message: String
}

enum RootAuthNetwork {
    private static let frameMagic: UInt32 = 0x4D495746
    private static let frameVersion: UInt16 = 1
    private static let frameHeaderSize = 12
    private static let maxPayloadBytes = 16 * 1024 * 1024
    private static let typeQrLoginApproveRoot: UInt16 = 61

    static func approveQrLogin(
        username: String,
        qrId: String,
        secretHex: String,
        deviceId: String,
        rootCode: String,
        host: String,
        port: Int,
        useTls: Bool,
        fingerprint: String?
    ) async -> QrLoginApproveResult {
        guard !username.isEmpty, !qrId.isEmpty, !secretHex.isEmpty,
              !deviceId.isEmpty, !rootCode.isEmpty else {
            return QrLoginApproveResult(success: false, error: "invalid approve payload")
        }
        guard port > 0, port <= 65535 else {
            return QrLoginApproveResult(success: false, error: "invalid server endpoint")
        }
        var payload = Data()
        guard writeString(username, into: &payload),
              writeString(qrId, into: &payload),
              writeString(secretHex, into: &payload),
              writeString(deviceId, into: &payload),
              writeString(rootCode, into: &payload) else {
            return QrLoginApproveResult(success: false, error: "payload encode failed")
        }
        let frame = encodeFrame(type: typeQrLoginApproveRoot, payload: payload)
        guard let endpointPort = NWEndpoint.Port(rawValue: UInt16(port)) else {
            return QrLoginApproveResult(success: false, error: "invalid server port")
        }
        let params = makeParameters(useTls: useTls, fingerprint: fingerprint)
        return await withCheckedContinuation { continuation in
            let queue = DispatchQueue.global(qos: .userInitiated)
            let connection = NWConnection(host: NWEndpoint.Host(host),
                                          port: endpointPort,
                                          using: params)
            var finished = false
            func finish(_ result: QrLoginApproveResult) {
                if finished { return }
                finished = true
                continuation.resume(returning: result)
                connection.cancel()
            }
            connection.stateUpdateHandler = { state in
                switch state {
                case .ready:
                    connection.send(content: frame, completion: .contentProcessed { error in
                        if let error = error {
                            finish(QrLoginApproveResult(success: false,
                                                        error: error.localizedDescription))
                            return
                        }
                        receiveExact(connection, length: frameHeaderSize, buffer: Data()) { headerResult in
                            switch headerResult {
                            case .failure(let err):
                                finish(QrLoginApproveResult(success: false, error: err.message))
                            case .success(let header):
                                guard header.count == frameHeaderSize else {
                                    finish(QrLoginApproveResult(success: false, error: "response header invalid"))
                                    return
                                }
                                let magic = readUInt32LE(header, offset: 0)
                                let version = readUInt16LE(header, offset: 4)
                                let respType = readUInt16LE(header, offset: 6)
                                let payloadLen = Int(readUInt32LE(header, offset: 8))
                                if magic != frameMagic || version != frameVersion {
                                    finish(QrLoginApproveResult(success: false, error: "response header invalid"))
                                    return
                                }
                                if respType != typeQrLoginApproveRoot {
                                    finish(QrLoginApproveResult(success: false, error: "response type mismatch"))
                                    return
                                }
                                if payloadLen < 0 || payloadLen > maxPayloadBytes {
                                    finish(QrLoginApproveResult(success: false, error: "response payload invalid"))
                                    return
                                }
                                receiveExact(connection, length: payloadLen, buffer: Data()) { payloadResult in
                                    switch payloadResult {
                                    case .failure(let err):
                                        finish(QrLoginApproveResult(success: false, error: err.message))
                                    case .success(let payload):
                                        finish(decodeApproveResult(payload))
                                    }
                                }
                            }
                        }
                    })
                case .failed(let error):
                    finish(QrLoginApproveResult(success: false,
                                                error: error.localizedDescription))
                case .cancelled:
                    if !finished {
                        finish(QrLoginApproveResult(success: false,
                                                    error: "connection cancelled"))
                    }
                default:
                    break
                }
            }
            connection.start(queue: queue)
            queue.asyncAfter(deadline: .now() + 8.0) {
                if !finished {
                    finish(QrLoginApproveResult(success: false, error: "timeout"))
                }
            }
        }
    }

    static func decodeApproveResultForSmoke(_ payload: Data) -> QrLoginApproveResult {
        decodeApproveResult(payload)
    }

    private static func decodeApproveResult(_ payload: Data) -> QrLoginApproveResult {
        guard !payload.isEmpty else {
            return QrLoginApproveResult(success: false, error: "response empty")
        }
        let ok = payload[payload.startIndex] != 0
        if ok {
            return QrLoginApproveResult(success: true, error: nil)
        }
        let err = readString(payload, offset: 1) ?? "approve failed"
        return QrLoginApproveResult(success: false, error: err)
    }

    private static func receiveExact(
        _ connection: NWConnection,
        length: Int,
        buffer: Data,
        completion: @escaping (Result<Data, RootAuthNetworkReceiveError>) -> Void
    ) {
        if length == 0 {
            completion(.success(Data()))
            return
        }
        let remaining = max(length - buffer.count, 0)
        if remaining == 0 {
            completion(.success(buffer))
            return
        }
        connection.receive(minimumIncompleteLength: 1, maximumLength: remaining) { data, _, isComplete, error in
            if let error = error {
                completion(.failure(RootAuthNetworkReceiveError(message: error.localizedDescription)))
                return
            }
            var next = buffer
            if let data = data {
                next.append(data)
            }
            if next.count >= length {
                completion(.success(Data(next.prefix(length))))
                return
            }
            if isComplete {
                completion(.failure(RootAuthNetworkReceiveError(message: "connection closed")))
                return
            }
            receiveExact(connection, length: length, buffer: next, completion: completion)
        }
    }

    private static func makeParameters(useTls: Bool, fingerprint: String?) -> NWParameters {
        guard useTls else { return NWParameters.tcp }
        let tlsOptions = NWProtocolTLS.Options()
        if let fp = normalizedFingerprint(fingerprint) {
            let expected = fp
            sec_protocol_options_set_verify_block(tlsOptions.securityProtocolOptions, { _, trust, complete in
                var ok = false
                let secTrust = sec_trust_copy_ref(trust).takeRetainedValue()
                if SecTrustGetCertificateCount(secTrust) > 0,
                   let cert = SecTrustGetCertificateAtIndex(secTrust, 0) {
                    let certData = SecCertificateCopyData(cert) as Data
                    let actual = sha256FingerprintHex(certificateData: certData)
                    ok = (actual == expected)
                }
                complete(ok)
            }, DispatchQueue.global(qos: .userInitiated))
        }
        return NWParameters(tls: tlsOptions)
    }

    static func normalizedFingerprint(_ fingerprint: String?) -> String? {
        guard let normalized = fingerprint?.trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased(),
            !normalized.isEmpty else {
            return nil
        }
        return normalized
    }

    static func sha256FingerprintHex(certificateData: Data) -> String {
        let digest = SHA256.hash(data: certificateData)
        return digest.map { String(format: "%02x", $0) }.joined()
    }

    private static func writeString(_ value: String, into data: inout Data) -> Bool {
        guard let bytes = value.data(using: .utf8) else { return false }
        guard bytes.count <= 0xFFFF else { return false }
        let len = UInt16(bytes.count)
        data.append(UInt8(len & 0xFF))
        data.append(UInt8((len >> 8) & 0xFF))
        data.append(bytes)
        return true
    }

    private static func readString(_ data: Data, offset: Int) -> String? {
        guard offset + 2 <= data.count else { return nil }
        let len = Int(data[offset]) | (Int(data[offset + 1]) << 8)
        let start = offset + 2
        let end = start + len
        guard end <= data.count else { return nil }
        return String(data: data.subdata(in: start..<end), encoding: .utf8)
    }

    private static func encodeFrame(type: UInt16, payload: Data) -> Data {
        var out = Data()
        out.reserveCapacity(frameHeaderSize + payload.count)
        appendUInt32LE(frameMagic, into: &out)
        appendUInt16LE(frameVersion, into: &out)
        appendUInt16LE(type, into: &out)
        appendUInt32LE(UInt32(payload.count), into: &out)
        out.append(payload)
        return out
    }

    private static func appendUInt16LE(_ value: UInt16, into data: inout Data) {
        data.append(UInt8(value & 0xFF))
        data.append(UInt8((value >> 8) & 0xFF))
    }

    private static func appendUInt32LE(_ value: UInt32, into data: inout Data) {
        data.append(UInt8(value & 0xFF))
        data.append(UInt8((value >> 8) & 0xFF))
        data.append(UInt8((value >> 16) & 0xFF))
        data.append(UInt8((value >> 24) & 0xFF))
    }

    private static func readUInt16LE(_ data: Data, offset: Int) -> UInt16 {
        return UInt16(data[offset]) | (UInt16(data[offset + 1]) << 8)
    }

    private static func readUInt32LE(_ data: Data, offset: Int) -> UInt32 {
        return UInt32(data[offset]) |
            (UInt32(data[offset + 1]) << 8) |
            (UInt32(data[offset + 2]) << 16) |
            (UInt32(data[offset + 3]) << 24)
    }
}
