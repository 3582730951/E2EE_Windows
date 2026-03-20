import Foundation
import CryptoKit

@inline(__always)
func require(_ condition: @autoclosure () -> Bool, _ message: String) {
    if !condition() {
        fputs("ios_rootauth_smoke: \(message)\n", stderr)
        exit(1)
    }
}

@main
struct RootAuthSmokeMain {
    static func main() {
        require(RootAuthNetwork.normalizedFingerprint(nil) == nil, "nil fingerprint should stay nil")
        require(RootAuthNetwork.normalizedFingerprint("  AbCd  ") == "abcd",
                "fingerprint normalization should trim and lowercase")

        let sample = Data([0x01, 0x02, 0x03, 0x04])
        let expectedDigest = SHA256.hash(data: sample).map { String(format: "%02x", $0) }.joined()
        require(RootAuthNetwork.sha256FingerprintHex(certificateData: sample) == expectedDigest,
                "fingerprint digest mismatch")

        let success = RootAuthNetwork.decodeApproveResultForSmoke(Data([1]))
        require(success.success && success.error == nil, "success payload decode failed")

        var failurePayload = Data([0, 3, 0])
        failurePayload.append("bad".data(using: .utf8)!)
        let failure = RootAuthNetwork.decodeApproveResultForSmoke(failurePayload)
        require(!failure.success && failure.error == "bad", "failure payload decode failed")

        print("ROOTAUTH_SMOKE_OK")
    }
}
