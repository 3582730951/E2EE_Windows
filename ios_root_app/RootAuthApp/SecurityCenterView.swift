import SwiftUI
import UIKit

struct SecurityCenterView: View {
    @ObservedObject var clientStore: ClientWorkspaceStore
    @ObservedObject var rootAuthStore: RootAuthStore

    private var transportTone: SecureBannerTone {
        if !clientStore.lastError.isEmpty && !clientStore.remoteOK {
            return .danger
        }
        return clientStore.remoteOK ? .success : .warning
    }

    private var transportIcon: String {
        switch transportTone {
        case .success:
            return "lock.shield.fill"
        case .warning:
            return "arrow.triangle.2.circlepath.circle.fill"
        case .danger:
            return "exclamationmark.shield.fill"
        case .neutral:
            return "shield"
        }
    }

    private var transportTitle: String {
        switch transportTone {
        case .success:
            return "Secure transport healthy"
        case .warning:
            return "Secure transport checking"
        case .danger:
            return "Transport attention required"
        case .neutral:
            return "Transport status"
        }
    }

    private var transportDetail: String {
        if !clientStore.lastError.isEmpty {
            return clientStore.lastError
        }
        if clientStore.remoteOK {
            return "Pinned transport and active session look healthy."
        }
        return "Waiting for secure transport validation."
    }

    private var currentDeviceLabel: String {
        clientStore.deviceDisplayID.isEmpty ? "Unavailable" : clientStore.deviceDisplayID
    }

    private var linkedDeviceSummaries: [String] {
        let current = currentDeviceLabel
        return clientStore.deviceSummaries.filter { summary in
            guard current != "Unavailable" else {
                return true
            }
            return !summary.localizedCaseInsensitiveContains(current)
        }
    }

    private var deviceCountLabel: String {
        let currentCount = currentDeviceLabel == "Unavailable" ? 0 : 1
        let total = currentCount + linkedDeviceSummaries.count
        switch total {
        case 0:
            return "No device data"
        case 1:
            return "1 device"
        default:
            return "\(total) devices"
        }
    }

    private var rootAuthStatusLabel: String {
        rootAuthStore.publicKeyHex.isEmpty ? "Not configured" : "Configured"
    }

    var body: some View {
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(alignment: .leading, spacing: 10) {
                    Text("SECURITY OVERVIEW")
                        .font(.caption.weight(.semibold))
                        .tracking(1.0)
                        .foregroundStyle(SecurePalette.textMuted)
                        .padding(.horizontal, 4)

                    summaryCard
                    devicesCard
                    transportCard
                    rootAuthCard
                }
                .padding(.horizontal, 14)
                .padding(.vertical, 16)
            }
        }
        .navigationTitle("Security Center")
        .navigationBarTitleDisplayMode(.inline)
        .tint(SecurePalette.accent)
    }

    private var summaryCard: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .top, spacing: 12) {
                VStack(alignment: .leading, spacing: 6) {
                    Text("SECURITY CENTER")
                        .font(.caption.weight(.semibold))
                        .tracking(1.2)
                        .foregroundStyle(SecurePalette.textMuted)
                    Text("Trust, devices, and session health")
                        .font(.title3.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text("Transport, linked devices, and root approval stay grouped here instead of competing with the main Settings list.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }

                Spacer(minLength: 12)

                Button(action: { clientStore.refreshNow() }) {
                    Image(systemName: "arrow.clockwise")
                        .font(.footnote.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                        .frame(width: 44, height: 44)
                        .background(
                            Circle()
                                .fill(SecurePalette.surfaceRaised)
                        )
                        .overlay(
                            Circle()
                                .stroke(SecurePalette.borderStrong, lineWidth: 1)
                        )
                }
                .buttonStyle(.plain)
                .accessibilityLabel("Refresh security state")
            }

            SecureStatusBanner(
                title: transportTitle,
                detail: transportDetail,
                tone: transportTone,
                systemImage: transportIcon
            )

            HStack(spacing: 10) {
                SecureMetricTile(
                    label: "Current device",
                    value: currentDeviceLabel,
                    icon: "iphone.gen3",
                    monospaced: true
                )
                SecureMetricTile(
                    label: "Linked devices",
                    value: deviceCountLabel,
                    icon: "macbook.and.iphone"
                )
            }

            SecureMetricTile(
                label: "Root auth",
                value: rootAuthStatusLabel,
                icon: "key.horizontal",
                monospaced: rootAuthStore.publicKeyHex.isEmpty
            )
        }
        .secureCard(padding: 16)
    }

    private var devicesCard: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Devices",
                title: "Devices and sessions",
                detail: "The active device stays visible. Other linked devices are listed below with short, scannable labels."
            )

            if currentDeviceLabel != "Unavailable" {
                deviceRow(
                    summary: currentDeviceLabel,
                    subtitle: "Current trusted device",
                    systemImage: "iphone.gen3",
                    highlight: true,
                    monospaced: true
                )
            }

            if linkedDeviceSummaries.isEmpty {
                SecureStatusBanner(
                    title: "Only this device is active",
                    detail: "No additional linked sessions are currently available from the client bridge.",
                    tone: .neutral,
                    systemImage: "iphone.gen3"
                )
            } else {
                VStack(spacing: 10) {
                    ForEach(Array(linkedDeviceSummaries.enumerated()), id: \.offset) { _, summary in
                        deviceRow(
                            summary: summary,
                            subtitle: "Linked session",
                            systemImage: "ipad.and.iphone",
                            highlight: false,
                            monospaced: false
                        )
                    }
                }
            }
        }
        .secureCard()
    }

    private var transportCard: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Transport",
                title: "Session transport and validation",
                detail: "Security state stays as a small chip in chat. This page is where details expand when operators need proof."
            )

            HStack(spacing: 10) {
                SecureMetricTile(
                    label: "Session",
                    value: clientStore.remoteOK ? "Encrypted" : "Verifying",
                    icon: "lock.square.stack"
                )
                SecureMetricTile(
                    label: "Status",
                    value: clientStore.lastError.isEmpty ? "No alerts" : "Needs review",
                    icon: "checkmark.seal"
                )
            }

            HStack(spacing: 10) {
                Button(action: { clientStore.refreshNow() }) {
                    Label("Refresh state", systemImage: "arrow.clockwise")
                }
                .buttonStyle(SecurePrimaryButtonStyle())

                Button(action: {
                    UIPasteboard.general.string = currentDeviceLabel
                }) {
                    Label("Copy device", systemImage: "doc.on.doc")
                }
                .buttonStyle(SecureSecondaryButtonStyle())
            }
        }
        .secureCard()
    }

    private var rootAuthCard: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Root Auth",
                title: "Approval identity",
                detail: "Root authorization remains available as a device-approval tool, but the operational flow now lives one level deeper."
            )

            if rootAuthStore.publicKeyHex.isEmpty {
                SecureStatusBanner(
                    title: "Root auth not configured",
                    detail: "Generate a signing identity before approving linked-device requests from this phone.",
                    tone: .warning,
                    systemImage: "key.viewfinder"
                )
            } else {
                SecureStatusBanner(
                    title: "Root auth signer configured",
                    detail: "Approval tools are ready when a linked device needs verification.",
                    tone: .success,
                    systemImage: "checkmark.shield"
                )

                SecureMetricTile(
                    label: "Public key",
                    value: shortHex(rootAuthStore.publicKeyHex),
                    icon: "key.horizontal",
                    monospaced: true
                )
            }

            HStack(spacing: 10) {
                Button(action: { UIPasteboard.general.string = rootAuthStore.publicKeyHex }) {
                    Label("Copy key", systemImage: "key")
                }
                .buttonStyle(SecurePrimaryButtonStyle())
                .disabled(rootAuthStore.publicKeyHex.isEmpty)
            }

            NavigationLink(destination: ContentView(store: rootAuthStore,
                                                   embeddedTitle: "Root authorization tools")) {
                Label("Open approval tools", systemImage: "qrcode.viewfinder")
            }
            .buttonStyle(SecureSecondaryButtonStyle())
        }
        .secureCard()
    }

    private func shortHex(_ value: String) -> String {
        guard value.count > 16 else {
            return value
        }
        return "\(value.prefix(12))...\(value.suffix(8))"
    }

    private func deviceRow(summary: String,
                           subtitle: String,
                           systemImage: String,
                           highlight: Bool,
                           monospaced: Bool) -> some View {
        HStack(spacing: 12) {
            Image(systemName: systemImage)
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(highlight ? SecurePalette.accent : SecurePalette.textSecondary)
                .frame(width: 30, height: 30)
                .background(
                    RoundedRectangle(cornerRadius: 10, style: .continuous)
                        .fill(SecurePalette.surfaceRaised)
                )

            VStack(alignment: .leading, spacing: 4) {
                Text(summary)
                    .font(monospaced ? .system(.subheadline, design: .monospaced).weight(.semibold)
                                     : .subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .lineLimit(2)
                Text(subtitle)
                    .font(.caption)
                    .foregroundStyle(SecurePalette.textSecondary)
            }

            Spacer(minLength: 0)
        }
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
