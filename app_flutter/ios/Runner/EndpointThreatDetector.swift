import Darwin
import Foundation
import ObjectiveC
import UIKit

struct EndpointThreatReport {
    let blocked: Bool
    let reasons: [String]
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
        if !injectedImageMarkers().isEmpty {
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

enum EndpointThreatBlocker {
    static func install(report: EndpointThreatReport) {
        let controller = UIViewController()
        controller.view.backgroundColor = UIColor(red: 0.02, green: 0.03, blue: 0.06, alpha: 1.0)

        let icon = UIImageView(image: UIImage(systemName: "shield.slash"))
        icon.tintColor = UIColor(red: 0.86, green: 0.16, blue: 0.16, alpha: 1.0)
        icon.translatesAutoresizingMaskIntoConstraints = false

        let title = UILabel()
        title.text = "Endpoint protection active"
        title.textColor = .white
        title.font = UIFont.preferredFont(forTextStyle: .headline)
        title.textAlignment = .center
        title.translatesAutoresizingMaskIntoConstraints = false

        let detail = UILabel()
        detail.text = report.reasons.joined(separator: " / ")
        detail.textColor = UIColor(white: 0.82, alpha: 1.0)
        detail.font = UIFont.preferredFont(forTextStyle: .footnote)
        detail.textAlignment = .center
        detail.numberOfLines = 3
        detail.translatesAutoresizingMaskIntoConstraints = false

        controller.view.addSubview(icon)
        controller.view.addSubview(title)
        controller.view.addSubview(detail)
        NSLayoutConstraint.activate([
            icon.centerXAnchor.constraint(equalTo: controller.view.centerXAnchor),
            icon.centerYAnchor.constraint(equalTo: controller.view.centerYAnchor, constant: -40),
            icon.widthAnchor.constraint(equalToConstant: 44),
            icon.heightAnchor.constraint(equalToConstant: 44),
            title.topAnchor.constraint(equalTo: icon.bottomAnchor, constant: 16),
            title.leadingAnchor.constraint(equalTo: controller.view.leadingAnchor, constant: 24),
            title.trailingAnchor.constraint(equalTo: controller.view.trailingAnchor, constant: -24),
            detail.topAnchor.constraint(equalTo: title.bottomAnchor, constant: 8),
            detail.leadingAnchor.constraint(equalTo: controller.view.leadingAnchor, constant: 24),
            detail.trailingAnchor.constraint(equalTo: controller.view.trailingAnchor, constant: -24)
        ])

        let window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = controller
        window.makeKeyAndVisible()
        objc_setAssociatedObject(controller, "mi_e2ee_endpoint_block_window", window, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }
}
