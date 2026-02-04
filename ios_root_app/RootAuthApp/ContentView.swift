import SwiftUI
import AVFoundation

struct ContentView: View {
    @StateObject private var store = RootAuthStore()
    @State private var showSecretSheet = false
    @State private var secretInput = ""
    @State private var scanResult: QrLoginPayload?
    @State private var scanError: String?
    @State private var showScanner = false
    @State private var hasCameraPermission = false

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 16) {
                    codeCard
                    actionRow
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
        .sheet(isPresented: $showSecretSheet) {
            secretSheet
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
            Text("Refresh in %d seconds".formatted(store.secondsRemaining))
                .font(.footnote)
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(16)
        .background(Color(UIColor.secondarySystemBackground))
        .cornerRadius(16)
    }

    private var actionRow: some View {
        HStack(spacing: 12) {
            Button(action: copyCode) {
                Label("Copy", systemImage: "doc.on.doc")
            }
            .buttonStyle(.borderedProminent)

            Button(action: { showSecretSheet = true }) {
                Label("Set secret", systemImage: "key")
            }
            .buttonStyle(.bordered)

            Button(action: store.clearSecret) {
                Label("Clear", systemImage: "trash")
            }
            .buttonStyle(.bordered)
        }
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
            Text(result.qrId.isEmpty ? "Root secret detected" : "New device login request")
                .font(.headline)
            if !result.qrId.isEmpty {
                Text("QR ID: %s".formatted(result.qrId))
                    .font(.footnote)
                if let device = result.deviceId {
                    Text("Device: %s".formatted(device))
                        .font(.footnote)
                }
                Text("Enter the auth code (or auth string) on the new device to finish.")
                    .font(.footnote)
                    .foregroundColor(.secondary)
                Button(action: copyCode) {
                    Label("Copy auth code", systemImage: "doc.on.doc")
                }
                .buttonStyle(.bordered)
                if let device = result.deviceId,
                   let proof = store.authProof(deviceId: device),
                   store.currentCode != "------" {
                    let authString = "%s:%s".formatted(store.currentCode, proof)
                    Button(action: {
                        UIPasteboard.general.string = authString
                    }) {
                        Label("Copy auth string", systemImage: "doc.on.doc")
                    }
                    .buttonStyle(.bordered)
                }
            } else {
                Text("Save this secret and start generating codes?")
                    .font(.footnote)
                Button(action: {
                    if store.setSecret(hex: result.secretHex) {
                        scanResult = nil
                    } else {
                        scanError = "Invalid secret format"
                    }
                }) {
                    Label("Save secret", systemImage: "checkmark.circle")
                }
                .buttonStyle(.borderedProminent)
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

    private var secretSheet: some View {
        NavigationView {
            VStack(spacing: 16) {
                Text("Enter 64-hex secret")
                    .font(.headline)
                TextField("Secret", text: $secretInput)
                    .textInputAutocapitalization(.never)
                    .disableAutocorrection(true)
                    .padding(12)
                    .background(Color(UIColor.secondarySystemBackground))
                    .cornerRadius(12)
                Button(action: {
                    if store.setSecret(hex: secretInput) {
                        secretInput = ""
                        showSecretSheet = false
                    } else {
                        scanError = "Invalid secret format"
                    }
                }) {
                    Text("Save")
                }
                .buttonStyle(.borderedProminent)
                Spacer()
            }
            .padding(16)
            .navigationTitle("Set secret")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Close") { showSecretSheet = false }
                }
            }
        }
    }

    private var scannerSheet: some View {
        NavigationView {
            VStack(spacing: 12) {
                if hasCameraPermission {
                    QrScannerView { code in
                        if let payload = QrLoginPayload.parse(code) {
                            scanResult = payload
                            scanError = nil
                        } else {
                            scanError = "QR code not recognized"
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

    private func startScan() {
        scanError = nil
        scanResult = nil
        showScanner = true
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
