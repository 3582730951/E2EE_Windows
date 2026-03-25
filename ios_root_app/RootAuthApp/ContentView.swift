import SwiftUI
import AVFoundation

struct ContentView: View {
    @ObservedObject var store: RootAuthStore
    @State private var scanResult: QrLoginPayload?
    @State private var scanError: String?
    @State private var showScanner = false
    @State private var hasCameraPermission = false
    @State private var approveStatus: String?
    @State private var approveError: String?
    @State private var isApproving = false
    @State private var manualDeviceId: String = ""
    var embeddedTitle: String? = nil

    var body: some View {
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(spacing: 20) {
                    if let embeddedTitle, !embeddedTitle.isEmpty {
                        SecureSectionHeader(
                            eyebrow: "Security Center",
                            title: embeddedTitle,
                            detail: "Approve new devices and manage short-lived root authorization codes from the main client shell."
                        )
                    }

                    heroCard

                    if let error = scanError {
                        SecureStatusBanner(
                            title: "Scan failed",
                            detail: error,
                            tone: .danger,
                            systemImage: "exclamationmark.shield"
                        )
                    }

                    manualApprovalCard
                    scanApprovalCard

                    if let result = scanResult {
                        scanResultCard(result)
                    }
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 20)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .tint(SecurePalette.accent)
        .preferredColorScheme(.dark)
        .sheet(isPresented: $showScanner) {
            scannerSheet
        }
        .onAppear {
            requestCameraPermission()
        }
    }

    private var heroCard: some View {
        VStack(alignment: .leading, spacing: 18) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("SECURE ROOT AUTH")
                        .font(.caption.weight(.semibold))
                        .tracking(1.2)
                        .foregroundStyle(SecurePalette.textMuted)
                    Text("Approve new devices with a calmer, verifiable flow.")
                        .font(.title3.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text("This device signs short-lived approvals and exposes only the material an operator needs to verify before accepting a login.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }

                Spacer(minLength: 12)

                HStack(spacing: 8) {
                    Image(systemName: "checkmark.shield.fill")
                    Text("This iPhone")
                }
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(
                    Capsule()
                        .fill(SecurePalette.surfaceRaised)
                )
            }

            VStack(alignment: .leading, spacing: 10) {
                Text("Approval code")
                    .font(.subheadline.weight(.medium))
                    .foregroundStyle(SecurePalette.textSecondary)
                Text(store.currentCode)
                    .font(.system(size: 46, weight: .bold, design: .monospaced))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                Text("Refreshes in \(store.secondsRemaining)s")
                    .font(.footnote.weight(.medium))
                    .foregroundStyle(SecurePalette.accent)
            }
            .padding(18)
            .background(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )

            HStack(spacing: 12) {
                SecureMetricTile(
                    label: "Public key",
                    value: store.publicKeyHex.isEmpty ? "Unavailable" : shortHex(store.publicKeyHex),
                    icon: "key.horizontal",
                    monospaced: true
                )
                SecureMetricTile(
                    label: "Rotation",
                    value: "\(store.secondsRemaining)s left",
                    icon: "timer",
                    monospaced: true
                )
            }

            HStack(spacing: 12) {
                Button(action: copyCode) {
                    Label("Copy code", systemImage: "doc.on.doc")
                }
                .buttonStyle(SecurePrimaryButtonStyle())

                Button(action: copyPublicKey) {
                    Label("Copy key", systemImage: "key")
                }
                .buttonStyle(SecureSecondaryButtonStyle())

                Button(action: store.regenerateKey) {
                    Label("Rotate", systemImage: "arrow.triangle.2.circlepath")
                }
                .buttonStyle(SecureSecondaryButtonStyle())
            }
        }
        .secureCard(padding: 20)
    }

    private var manualApprovalCard: some View {
        let trimmedID = manualDeviceId.trimmingCharacters(in: .whitespacesAndNewlines)
        let authString = trimmedID.isEmpty ? "" : (store.authString(deviceId: trimmedID, context: "device_register") ?? "")

        return VStack(alignment: .leading, spacing: 16) {
            SecureSectionHeader(
                eyebrow: "Manual fallback",
                title: "Generate an approval string",
                detail: "Paste the device ID shown on the login screen when QR exchange is unavailable."
            )

            TextField("Device ID", text: $manualDeviceId)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled(true)
                .secureInput()

            if authString.isEmpty {
                SecureStatusBanner(
                    title: "Waiting for a device identifier",
                    detail: "Once a valid device ID is present, the signed auth string is generated locally on this phone.",
                    tone: .neutral,
                    systemImage: "key.viewfinder"
                )
            } else {
                VStack(alignment: .leading, spacing: 10) {
                    Text("Approval string")
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(SecurePalette.textSecondary)
                    Text(authString)
                        .font(.system(.footnote, design: .monospaced))
                        .foregroundStyle(SecurePalette.textPrimary)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(14)
                        .background(
                            RoundedRectangle(cornerRadius: 18, style: .continuous)
                                .fill(SecurePalette.surfaceRaised)
                        )
                        .overlay(
                            RoundedRectangle(cornerRadius: 18, style: .continuous)
                                .stroke(SecurePalette.border, lineWidth: 1)
                        )

                    Button(action: { UIPasteboard.general.string = authString }) {
                        Label("Copy approval string", systemImage: "doc.on.doc")
                    }
                    .buttonStyle(SecureSecondaryButtonStyle())
                }
            }
        }
        .secureCard()
    }

    private var scanApprovalCard: some View {
        VStack(alignment: .leading, spacing: 16) {
            SecureSectionHeader(
                eyebrow: "Fast lane",
                title: "Scan and verify a login request",
                detail: "Use the camera when the requesting device presents a QR payload with host, TLS, and device metadata."
            )

            if hasCameraPermission {
                SecureStatusBanner(
                    title: "Camera ready",
                    detail: "Open the scanner, inspect the request, then approve only after verifying the account and server fingerprint.",
                    tone: .success,
                    systemImage: "camera.metering.center.weighted"
                )
            } else {
                SecureStatusBanner(
                    title: "Camera permission required",
                    detail: "Enable camera access in Settings to review login QR requests directly from this device.",
                    tone: .warning,
                    systemImage: "camera.badge.ellipsis"
                )
            }

            Button(action: startScan) {
                Label("Open scanner", systemImage: "qrcode.viewfinder")
            }
            .buttonStyle(SecurePrimaryButtonStyle())
            .disabled(!hasCameraPermission)
        }
        .secureCard()
    }

    private func scanResultCard(_ result: QrLoginPayload) -> some View {
        let context = "qr:\(result.qrId):\(result.secretHex.lowercased())"
        let authString = result.deviceId.flatMap { store.authString(deviceId: $0, context: context) } ?? ""
        let canApprove = !authString.isEmpty &&
            result.username != nil &&
            result.deviceId != nil &&
            result.host != nil &&
            result.port != nil

        return VStack(alignment: .leading, spacing: 16) {
            SecureSectionHeader(
                eyebrow: "Pending request",
                title: "Review this device before approving",
                detail: "Treat the QR as untrusted input until account, device, server, and TLS details all match the intended login."
            )

            if result.qrId.isEmpty {
                SecureStatusBanner(
                    title: "Incomplete QR payload",
                    detail: "The scanned payload does not include a valid login identifier.",
                    tone: .danger,
                    systemImage: "exclamationmark.triangle.fill"
                )
            } else {
                HStack(spacing: 12) {
                    SecureMetricTile(label: "Account", value: result.username ?? "Missing", icon: "person.crop.circle")
                    SecureMetricTile(label: "Device", value: result.deviceId ?? "Missing", icon: "iphone.gen3")
                }

                HStack(spacing: 12) {
                    SecureMetricTile(label: "Server", value: serverLabel(for: result), icon: "server.rack")
                    SecureMetricTile(label: "Transport", value: result.useTls ? "TLS pinned" : "Plain TCP", icon: "lock.shield")
                }

                if let fingerprint = result.fingerprint, !fingerprint.isEmpty {
                    SecureStatusBanner(
                        title: "Pinned fingerprint present",
                        detail: shortHex(fingerprint),
                        tone: .neutral,
                        systemImage: "lock.doc"
                    )
                }

                VStack(alignment: .leading, spacing: 10) {
                    Text("Approval string")
                        .font(.subheadline.weight(.medium))
                        .foregroundStyle(SecurePalette.textSecondary)

                    if authString.isEmpty {
                        SecureStatusBanner(
                            title: "Root key unavailable",
                            detail: "This device could not derive a signed approval string for the scanned request.",
                            tone: .danger,
                            systemImage: "key.slash"
                        )
                    } else {
                        Text(authString)
                            .font(.system(.footnote, design: .monospaced))
                            .foregroundStyle(SecurePalette.textPrimary)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(14)
                            .background(
                                RoundedRectangle(cornerRadius: 18, style: .continuous)
                                    .fill(SecurePalette.surfaceRaised)
                            )
                            .overlay(
                                RoundedRectangle(cornerRadius: 18, style: .continuous)
                                    .stroke(SecurePalette.border, lineWidth: 1)
                            )
                    }
                }

                HStack(spacing: 12) {
                    Button(action: copyCode) {
                        Label("Copy code", systemImage: "number.square")
                    }
                    .buttonStyle(SecureSecondaryButtonStyle())

                    if !authString.isEmpty {
                        Button(action: { UIPasteboard.general.string = authString }) {
                            Label("Copy auth string", systemImage: "doc.on.doc")
                        }
                        .buttonStyle(SecureSecondaryButtonStyle())
                    }
                }

                Button(action: {
                    approveLogin(result, rootCode: authString)
                }) {
                    if isApproving {
                        Label("Authorizing...", systemImage: "hourglass")
                    } else {
                        Label("Approve this login", systemImage: "checkmark.shield")
                    }
                }
                .buttonStyle(SecurePrimaryButtonStyle())
                .disabled(!canApprove || isApproving)

                if let status = approveStatus {
                    SecureStatusBanner(
                        title: status,
                        detail: "The requesting device can now finish its registration flow using the signed response.",
                        tone: .success,
                        systemImage: "checkmark.circle.fill"
                    )
                }

                if let err = approveError {
                    SecureStatusBanner(
                        title: "Approval failed",
                        detail: err,
                        tone: .danger,
                        systemImage: "xmark.circle.fill"
                    )
                }
            }
        }
        .secureCard()
    }

    private var scannerSheet: some View {
        NavigationStack {
            ZStack {
                SecureSceneBackground()

                VStack(spacing: 16) {
                    if hasCameraPermission {
                        QrScannerView { code in
                            if let payload = QrLoginPayload.parse(code) {
                                scanResult = payload
                                scanError = nil
                                approveStatus = nil
                                approveError = nil
                                isApproving = false
                            } else {
                                scanError = "QR code not recognized."
                                approveStatus = nil
                                approveError = nil
                            }
                            showScanner = false
                        }
                        .clipShape(RoundedRectangle(cornerRadius: 28, style: .continuous))
                        .overlay(
                            RoundedRectangle(cornerRadius: 28, style: .continuous)
                                .stroke(SecurePalette.borderStrong, lineWidth: 1)
                        )
                    } else {
                        SecureStatusBanner(
                            title: "Camera unavailable",
                            detail: "Allow camera access in Settings, then reopen the scanner.",
                            tone: .warning,
                            systemImage: "camera.aperture"
                        )
                    }

                    SecureStatusBanner(
                        title: "Verify before approving",
                        detail: "Match account, device, host, port, and TLS fingerprint before you authorize a new device.",
                        tone: .neutral,
                        systemImage: "shield.lefthalf.filled"
                    )
                }
                .padding(18)
            }
            .navigationTitle("Scan Request")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Close") { showScanner = false }
                }
            }
        }
        .tint(SecurePalette.accent)
        .preferredColorScheme(.dark)
    }

    private func copyCode() {
        UIPasteboard.general.string = store.currentCode
    }

    private func copyPublicKey() {
        UIPasteboard.general.string = store.publicKeyHex
    }

    private func startScan() {
        scanError = nil
        scanResult = nil
        approveStatus = nil
        approveError = nil
        isApproving = false
        showScanner = true
    }

    private func approveLogin(_ result: QrLoginPayload, rootCode: String) {
        guard let host = result.host,
              let port = result.port,
              let username = result.username,
              let deviceId = result.deviceId else {
            approveError = "Server info missing in QR."
            return
        }
        if rootCode.isEmpty {
            approveError = "Root key unavailable."
            return
        }
        isApproving = true
        approveStatus = "Authorizing..."
        approveError = nil
        Task {
            let response = await RootAuthNetwork.approveQrLogin(
                username: username,
                qrId: result.qrId,
                secretHex: result.secretHex,
                deviceId: deviceId,
                rootCode: rootCode,
                host: host,
                port: port,
                useTls: result.useTls,
                fingerprint: result.fingerprint
            )
            await MainActor.run {
                isApproving = false
                if response.success {
                    approveStatus = "Authorization sent."
                    approveError = nil
                } else {
                    approveStatus = nil
                    approveError = response.error ?? "Authorization failed."
                }
            }
        }
    }

    private func requestCameraPermission() {
        switch AVCaptureDevice.authorizationStatus(for: .video) {
        case .authorized:
            hasCameraPermission = true
        case .notDetermined:
            AVCaptureDevice.requestAccess(for: .video) { granted in
                DispatchQueue.main.async {
                    hasCameraPermission = granted
                }
            }
        default:
            hasCameraPermission = false
        }
    }

    private func shortHex(_ value: String) -> String {
        guard value.count > 20 else {
            return value
        }
        return "\(value.prefix(10))...\(value.suffix(8))"
    }

    private func serverLabel(for result: QrLoginPayload) -> String {
        guard let host = result.host, let port = result.port else {
            return "Missing"
        }
        return "\(host):\(port)"
    }
}
