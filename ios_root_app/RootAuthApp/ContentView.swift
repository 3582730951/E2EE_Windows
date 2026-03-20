import SwiftUI
import AVFoundation

struct ContentView: View {
    @StateObject private var store = RootAuthStore()
    @State private var scanResult: QrLoginPayload?
    @State private var scanError: String?
    @State private var showScanner = false
    @State private var hasCameraPermission = false
    @State private var approveStatus: String?
    @State private var approveError: String?
    @State private var isApproving = false
    @State private var manualDeviceId: String = ""

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 16) {
                    codeCard
                    actionRow
                    manualCard
                    scanCard
                    if let result = scanResult {
                        scanResultCard(result)
                    }
                    if let error = scanError {
                        errorCard(error)
                    }
                }
                .padding(16)
            }
            .navigationTitle("Root Auth")
        }
        .sheet(isPresented: $showScanner) {
            scannerSheet
        }
        .onAppear {
            requestCameraPermission()
        }
    }

    private var codeCard: some View {
        VStack(spacing: 10) {
            Text("Auth code")
                .font(.headline)
            Text(store.currentCode)
                .font(.system(size: 40, weight: .bold, design: .monospaced))
            Text("Refresh in \(store.secondsRemaining) seconds")
                .font(.footnote)
                .foregroundColor(.secondary)
            Divider()
            Text("Public key")
                .font(.headline)
            Text(store.publicKeyHex.isEmpty ? "Unavailable" : store.publicKeyHex)
                .font(.footnote)
                .foregroundColor(.secondary)
                .lineLimit(2)
        }
        .frame(maxWidth: .infinity)
        .padding(16)
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(16)
    }

    private var actionRow: some View {
        HStack(spacing: 12) {
            Button(action: copyCode) {
                Label("Copy code", systemImage: "doc.on.doc")
            }
            .buttonStyle(.borderedProminent)

            Button(action: copyPublicKey) {
                Label("Copy public key", systemImage: "key")
            }
            .buttonStyle(.bordered)

            Button(action: store.regenerateKey) {
                Label("Regenerate key", systemImage: "arrow.triangle.2.circlepath")
            }
            .buttonStyle(.bordered)
        }
    }

    private var manualCard: some View {
        let trimmedId = manualDeviceId.trimmingCharacters(in: .whitespacesAndNewlines)
        let authString = trimmedId.isEmpty
            ? ""
            : (store.authString(deviceId: trimmedId, context: "device_register") ?? "")
        return VStack(alignment: .leading, spacing: 8) {
            Text("Manual device authorization")
                .font(.headline)
            Text("Enter the device ID from the new device login screen to generate an auth string.")
                .font(.footnote)
                .foregroundColor(.secondary)
            TextField("Device ID", text: $manualDeviceId)
                .autocapitalization(.none)
                .disableAutocorrection(true)
                .textFieldStyle(.roundedBorder)
            if authString.isEmpty {
                Text("Auth string will appear here.")
                    .font(.footnote)
                    .foregroundColor(.secondary)
            } else {
                Text(authString)
                    .font(.footnote)
                Button(action: { UIPasteboard.general.string = authString }) {
                    Label("Copy auth string", systemImage: "doc.on.doc")
                }
                .buttonStyle(.bordered)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(16)
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(16)
    }

    private var scanCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Scan login QR")
                .font(.headline)
            Text("Scan the login QR, verify device info, then authorize.")
                .font(.footnote)
                .foregroundColor(.secondary)
            Button(action: startScan) {
                Label("Start scan", systemImage: "qrcode.viewfinder")
            }
            .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(16)
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(16)
    }

    private func scanResultCard(_ result: QrLoginPayload) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("New device login request")
                .font(.headline)
            if result.qrId.isEmpty {
                Text("QR payload missing login data.")
                    .font(.footnote)
                    .foregroundColor(.red)
            } else {
                Text("QR ID: \(result.qrId)")
                    .font(.footnote)
                if let username = result.username {
                    Text("Account: \(username)")
                        .font(.footnote)
                }
                if let device = result.deviceId {
                    Text("Device: \(device)")
                        .font(.footnote)
                }
                if let host = result.host, let port = result.port {
                    Text("Server: \(host):\(port)")
                        .font(.footnote)
                    Text(result.useTls ? "TLS: enabled" : "TLS: disabled")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                    if let fingerprint = result.fingerprint {
                        Text("Fingerprint: \(fingerprint)")
                            .font(.footnote)
                            .foregroundColor(.secondary)
                    }
                } else {
                    Text("Server info missing in QR.")
                        .font(.footnote)
                        .foregroundColor(.red)
                }
                Text("Enter the auth string (code + signature) on the new device.")
                    .font(.footnote)
                    .foregroundColor(.secondary)
                Button(action: copyCode) {
                    Label("Copy auth code", systemImage: "doc.on.doc")
                }
                .buttonStyle(.bordered)
                let context = "qr:\(result.qrId):\(result.secretHex.lowercased())"
                let authString = result.deviceId.flatMap { store.authString(deviceId: $0, context: context) } ?? ""
                if !authString.isEmpty {
                    Button(action: {
                        UIPasteboard.general.string = authString
                    }) {
                        Label("Copy auth string", systemImage: "doc.on.doc")
                    }
                    .buttonStyle(.bordered)
                }
                if authString.isEmpty {
                    Text("Root key unavailable.")
                        .font(.footnote)
                        .foregroundColor(.red)
                }
                let canApprove = !authString.isEmpty &&
                    result.username != nil &&
                    result.deviceId != nil &&
                    result.host != nil &&
                    result.port != nil
                Button(action: {
                    approveLogin(result, rootCode: authString)
                }) {
                    Label("Authorize login", systemImage: "checkmark.shield")
                }
                .buttonStyle(.borderedProminent)
                .disabled(!canApprove || isApproving)
                if isApproving {
                    ProgressView("Authorizing...")
                        .font(.footnote)
                }
                if let status = approveStatus {
                    Text(status)
                        .font(.footnote)
                        .foregroundColor(.green)
                }
                if let err = approveError {
                    Text(err)
                        .font(.footnote)
                        .foregroundColor(.red)
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(16)
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(16)
    }

    private func errorCard(_ message: String) -> some View {
        Text(message)
            .font(.footnote)
            .foregroundColor(.red)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(12)
            .background(Color.red.opacity(0.1))
            .cornerRadius(12)
    }

    private var scannerSheet: some View {
        NavigationView {
            VStack(spacing: 12) {
                if hasCameraPermission {
                    QrScannerView { code in
                    if let payload = QrLoginPayload.parse(code) {
                        scanResult = payload
                        scanError = nil
                        approveStatus = nil
                        approveError = nil
                        isApproving = false
                    } else {
                        scanError = "QR code not recognized"
                        approveStatus = nil
                        approveError = nil
                    }
                    showScanner = false
                }
                .cornerRadius(12)
                } else {
                    Text("Allow camera access in system settings.")
                        .font(.footnote)
                        .foregroundColor(.secondary)
                }
                Spacer()
            }
            .padding(16)
            .navigationTitle("Scan")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Close") { showScanner = false }
                }
            }
        }
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
}
