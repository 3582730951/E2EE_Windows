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
                VStack(spacing: 16) {
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
                .padding(.horizontal, 14)
                .padding(.vertical, 16)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .tint(SecurePalette.accent)
        .sheet(isPresented: $showScanner) {
            scannerSheet
        }
        .onAppear {
            requestCameraPermission()
        }
    }

    private var heroCard: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(alignment: .top, spacing: 14) {
                ZStack(alignment: .bottomTrailing) {
                    SecureIdentityAvatar(
                        title: "Root approval",
                        seed: "root-auth",
                        kind: .system,
                        size: 56,
                        presence: .secure,
                        prominent: true
                    )
                    SecureIdentityAvatar(
                        title: "This iPhone",
                        seed: "root-auth-device",
                        kind: .device,
                        size: 28
                    )
                    .offset(x: 5, y: 5)
                }

                VStack(alignment: .leading, spacing: 8) {
                    HStack(spacing: 8) {
                        SecureMetricBadge(
                            title: "Role",
                            value: "Approver",
                            systemImage: "checkmark.shield.fill",
                            accent: SecurePalette.success
                        )
                        SecureMetricBadge(
                            title: "Device",
                            value: "This iPhone",
                            systemImage: "iphone.gen3",
                            accent: SecurePalette.accentSky
                        )
                    }

                    Text("Approve new devices with visible rotation and signing state.")
                        .font(.title3.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)

                    Text("The root key never leaves this device. Operators only need the current code, public key, and the request snapshot before approving.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            VStack(alignment: .leading, spacing: 12) {
                HStack(alignment: .firstTextBaseline, spacing: 12) {
                    Text(store.currentCode)
                        .font(.system(size: 42, weight: .bold, design: .monospaced))
                        .foregroundStyle(SecurePalette.textPrimary)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)

                    Spacer(minLength: 0)

                    SecureMetricBadge(
                        title: "Rotation",
                        value: "\(store.secondsRemaining)s",
                        systemImage: "timer",
                        accent: SecurePalette.accentAmber
                    )
                }

                HStack(spacing: 12) {
                    SecureMetricTile(
                        label: "Public key",
                        value: store.publicKeyHex.isEmpty ? "Unavailable" : shortHex(store.publicKeyHex),
                        icon: "key.horizontal.fill",
                        monospaced: true,
                        accent: store.publicKeyHex.isEmpty ? SecurePalette.warning : SecurePalette.success
                    )
                    SecureMetricTile(
                        label: "Signing",
                        value: store.publicKeyHex.isEmpty ? "Missing" : "Ready",
                        icon: "signature",
                        accent: store.publicKeyHex.isEmpty ? SecurePalette.warning : SecurePalette.accentSky
                    )
                }
            }
            .padding(16)
            .background(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )

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
        .secureCard(padding: 18)
    }

    private var manualApprovalCard: some View {
        let trimmedID = manualDeviceId.trimmingCharacters(in: .whitespacesAndNewlines)
        let authString = trimmedID.isEmpty ? "" : (store.authString(deviceId: trimmedID, context: "device_register") ?? "")

        return VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Manual fallback",
                title: "Generate an approval string",
                detail: "Paste the device ID shown on the login screen when QR exchange is unavailable."
            )

            HStack(spacing: 12) {
                SecureIdentityAvatar(
                    title: trimmedID.isEmpty ? "Pending device" : trimmedID,
                    seed: trimmedID.isEmpty ? "pending-device" : trimmedID,
                    kind: .device,
                    size: 42,
                    presence: authString.isEmpty ? .review : .secure
                )

                VStack(alignment: .leading, spacing: 4) {
                    Text(trimmedID.isEmpty ? "Awaiting device ID" : "Device ready for approval")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text(authString.isEmpty ? "Approval string is generated locally once a valid device ID is present." : "Signed string generated locally on this phone.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }
            }

            TextField("Device ID", text: $manualDeviceId)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled(true)
                .secureInput()

            if authString.isEmpty {
                VStack(spacing: 10) {
                    SecureEmptyStateIllustration(systemImage: "iphone.gen3", accent: SecurePalette.accentSky, size: 82)
                    SecureStatusBanner(
                        title: "Waiting for a device identifier",
                        detail: "Once a valid device ID is present, the signed auth string is generated locally on this phone.",
                        tone: .neutral,
                        systemImage: "key.viewfinder"
                    )
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 4)
            } else {
                VStack(alignment: .leading, spacing: 10) {
                    approvalStringCard(authString)

                    Button(action: { SecureClipboard.copyProtectedText(authString) }) {
                        Label("Copy approval string", systemImage: "doc.on.doc")
                    }
                    .buttonStyle(SecureSecondaryButtonStyle())
                }
            }
        }
        .secureCard()
    }

    private var scanApprovalCard: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Fast lane",
                title: "Scan and verify a login request",
                detail: "Use the camera when the requesting device presents a QR payload with host, TLS, and device metadata."
            )

            HStack(spacing: 12) {
                SecureIdentityAvatar(
                    title: "QR approval",
                    seed: "qr-approval",
                    kind: .system,
                    size: 42,
                    presence: hasCameraPermission ? .secure : .review
                )

                VStack(alignment: .leading, spacing: 4) {
                    Text(hasCameraPermission ? "Camera ready" : "Camera permission required")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text(hasCameraPermission
                         ? "Inspect the request, then approve only after verifying account and server fingerprint."
                         : "Enable camera access in Settings to review login QR requests directly from this device.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }
            }

            SecureStatusBanner(
                title: hasCameraPermission ? "Scanner available" : "Scanner blocked",
                detail: hasCameraPermission
                    ? "QR approval keeps server, TLS, device, and approval context in a single review flow."
                    : "QR review is unavailable until camera access is granted.",
                tone: hasCameraPermission ? .success : .warning,
                systemImage: hasCameraPermission ? "camera.metering.center.weighted" : "camera.badge.ellipsis"
            )

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

        return VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Pending request",
                title: "Review this device before approving",
                detail: "Treat the QR as untrusted input until account, device, server, and TLS details all match the intended login."
            )

            HStack(spacing: 12) {
                SecureIdentityAvatar(
                    title: result.deviceId ?? "Unknown device",
                    seed: result.deviceId ?? "qr-device",
                    kind: .device,
                    size: 44,
                    presence: canApprove ? .secure : .review
                )

                VStack(alignment: .leading, spacing: 4) {
                    Text(result.deviceId ?? "Missing device ID")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text(result.username ?? "Missing account")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }
            }

            LazyVGrid(columns: [
                GridItem(.flexible(), spacing: 12),
                GridItem(.flexible(), spacing: 12)
            ], spacing: 12) {
                SecureMetricTile(
                    label: "Account",
                    value: result.username ?? "Missing",
                    icon: "person.crop.circle.fill",
                    accent: SecurePalette.accentSky
                )
                SecureMetricTile(
                    label: "Device",
                    value: result.deviceId ?? "Missing",
                    icon: "iphone.gen3",
                    monospaced: true,
                    accent: SecurePalette.accent
                )
                SecureMetricTile(
                    label: "Server",
                    value: serverLabel(for: result),
                    icon: "server.rack",
                    accent: SecurePalette.accentLavender
                )
                SecureMetricTile(
                    label: "Transport",
                    value: result.useTls ? "TLS pinned" : "Plain TCP",
                    icon: result.useTls ? "lock.shield.fill" : "antenna.radiowaves.left.and.right",
                    accent: result.useTls ? SecurePalette.success : SecurePalette.warning
                )
            }

            if let fingerprint = result.fingerprint, !fingerprint.isEmpty {
                SecureStatusBanner(
                    title: "Pinned fingerprint present",
                    detail: shortHex(fingerprint),
                    tone: .neutral,
                    systemImage: "lock.doc"
                )
            }

            if authString.isEmpty {
                SecureStatusBanner(
                    title: "Root key unavailable",
                    detail: "This device could not derive a signed approval string for the scanned request.",
                    tone: .danger,
                    systemImage: "key.slash"
                )
            } else {
                approvalStringCard(authString)
            }

            HStack(spacing: 12) {
                Button(action: copyCode) {
                    Label("Copy code", systemImage: "number.square")
                }
                .buttonStyle(SecureSecondaryButtonStyle())

                if !authString.isEmpty {
                    Button(action: { SecureClipboard.copyProtectedText(authString) }) {
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
                .padding(16)
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
    }

    private func copyCode() {
        SecureClipboard.copyProtectedText(store.currentCode)
    }

    private func copyPublicKey() {
        SecureClipboard.copyProtectedText(store.publicKeyHex)
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

    private func approvalStringCard(_ value: String) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                SecureMetricBadge(
                    title: "Signed",
                    value: "Approval string",
                    systemImage: "key.viewfinder",
                    accent: SecurePalette.accent
                )
            }

            Text(value)
                .font(.system(.footnote, design: .monospaced))
                .foregroundStyle(SecurePalette.textPrimary)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(12)
                .background(
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .fill(SecurePalette.surfaceRaised)
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 16, style: .continuous)
                        .stroke(SecurePalette.border, lineWidth: 1)
                )
        }
    }
}
