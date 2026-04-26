import Darwin
import Foundation
import SwiftUI

struct EndpointThreatReport {
    let blocked: Bool
    let reasons: [String]

    var summary: String {
        if reasons.isEmpty {
            return "Endpoint clear"
        }
        return reasons.joined(separator: " / ")
    }
}

enum EndpointThreatDetector {
    private static let P_TRACED: Int32 = 0x00000800
    private static let suspiciousImageMarkers = [
        "frida",
        "substrate",
        "libhooker",
        "substitute",
        "cycript",
        "cynject"
    ]

    static func evaluate() -> EndpointThreatReport {
        var reasons: [String] = []
        if isDebuggerAttached() {
            reasons.append("debugger")
        }
        let injected = injectedImageMarkers()
        if !injected.isEmpty {
            reasons.append("injected_dylib")
        }
        return EndpointThreatReport(blocked: !reasons.isEmpty,
                                    reasons: Array(Set(reasons)).sorted())
    }

    private static func isDebuggerAttached() -> Bool {
        var info = kinfo_proc()
        var mib: [Int32] = [CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()]
        var size = MemoryLayout<kinfo_proc>.stride
        let rc = sysctl(&mib, u_int(mib.count), &info, &size, nil, 0)
        if rc != 0 {
            return false
        }
        return (info.kp_proc.p_flag & P_TRACED) != 0
    }

    private static func injectedImageMarkers() -> [String] {
        var hits: [String] = []
        for index in 0..<_dyld_image_count() {
            guard let imageName = _dyld_get_image_name(index) else {
                continue
            }
            let lowered = String(cString: imageName).lowercased()
            for marker in suspiciousImageMarkers where lowered.contains(marker) {
                hits.append(marker)
            }
        }
        return hits
    }
}

struct EndpointThreatBlockedView: View {
    let report: EndpointThreatReport

    var body: some View {
        ZStack {
            SecureSceneBackground()
            VStack(spacing: 18) {
                Image(systemName: "shield.slash")
                    .font(.system(size: 42, weight: .semibold))
                    .foregroundStyle(SecurePalette.danger)
                Text("Endpoint protection active")
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                Text(report.summary)
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .multilineTextAlignment(.center)
                    .lineLimit(3)
            }
            .padding(24)
        }
    }
}
