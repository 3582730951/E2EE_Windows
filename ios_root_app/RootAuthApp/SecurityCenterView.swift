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
            return "Secure"
        case .warning:
            return "Checking"
        case .danger:
            return "Review"
        case .neutral:
            return "Transport"
        }
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

    private var rootAuthStatusLabel: String {
        rootAuthStore.publicKeyHex.isEmpty ? "Pending" : "Ready"
    }

    private var compactDeviceLabel: String {
        guard currentDeviceLabel.count > 14 else {
            return currentDeviceLabel
        }
        return "\(currentDeviceLabel.prefix(6))...\(currentDeviceLabel.suffix(4))"
    }

    var body: some View {
        List {
            Section {
                overviewCard
                    .listRowInsets(EdgeInsets(top: 6, leading: 0, bottom: 6, trailing: 0))
                    .listRowSeparator(.hidden)
                    .listRowBackground(Color.clear)
            }

            Section("Transport") {
                SecurityMetricRow(
                    title: "Session",
                    value: transportTitle,
                    systemImage: transportIcon,
                    accent: transportTone.accent,
                    detail: clientStore.lastError.isEmpty ? clientStore.statusText : clientStore.lastError
                )
                SecurityMetricRow(
                    title: "This device",
                    value: compactDeviceLabel,
                    systemImage: "iphone.gen3",
                    accent: SecurePalette.accentSky,
                    detail: currentDeviceLabel
                )
                SecurityMetricRow(
                    title: "Linked devices",
                    value: "\(linkedDeviceSummaries.count)",
                    systemImage: "ipad.and.iphone",
                    accent: SecurePalette.accentLavender,
                    detail: linkedDeviceSummaries.isEmpty ? "No extra devices reported" : "Trusted companion devices"
                )
                SecurityMetricRow(
                    title: "Root auth",
                    value: rootAuthStatusLabel,
                    systemImage: "key.horizontal.fill",
                    accent: rootAuthStore.publicKeyHex.isEmpty ? SecurePalette.warning : SecurePalette.success,
                    detail: rootAuthStore.publicKeyHex.isEmpty ? "Public key not loaded" : "Approval material available"
                )
            }

            Section("Current Device") {
                currentDeviceCard
                    .listRowInsets(EdgeInsets(top: 4, leading: 0, bottom: 4, trailing: 0))
                    .listRowSeparator(.hidden)
                    .listRowBackground(Color.clear)
            }

            Section("Linked Devices") {
                if linkedDeviceSummaries.isEmpty {
                    VStack(spacing: 10) {
                        SecureEmptyStateIllustration(systemImage: "ipad.and.iphone", accent: SecurePalette.accentLavender, size: 82)
                        Text("No linked devices")
                            .font(.footnote.weight(.semibold))
                            .foregroundStyle(SecurePalette.textSecondary)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 6)
                } else {
                    ForEach(linkedDeviceSummaries, id: \.self) { summary in
                        SecurityLinkedDeviceRow(summary: summary)
                    }
                }
            }

            Section("Approval") {
                SecurityMetricRow(
                    title: "Approval code",
                    value: rootAuthStore.currentCode,
                    systemImage: "number.square.fill",
                    accent: SecurePalette.accent,
                    detail: "One-time root authorization code",
                    monospaced: true
                )
                SecurityMetricRow(
                    title: "Expires in",
                    value: "\(rootAuthStore.secondsRemaining)s",
                    systemImage: "timer",
                    accent: SecurePalette.accentAmber,
                    detail: "Time remaining before the current code rotates",
                    monospaced: true
                )
                SecurityMetricRow(
                    title: "Public key",
                    value: rootAuthStore.publicKeyHex.isEmpty ? "Unavailable" : shortHex(rootAuthStore.publicKeyHex),
                    systemImage: "key.horizontal.fill",
                    accent: rootAuthStore.publicKeyHex.isEmpty ? SecurePalette.warning : SecurePalette.success,
                    detail: rootAuthStore.publicKeyHex.isEmpty ? "Root authorization material missing" : "Pinned root authorization fingerprint",
                    monospaced: true
                )

                NavigationLink(destination: ContentView(store: rootAuthStore,
                                                       embeddedTitle: "Root authorization tools")) {
                    HStack(spacing: 12) {
                        SecureIdentityAvatar(
                            title: "Approval tools",
                            seed: "root-auth-tools",
                            kind: .system,
                            size: 38,
                            presence: .secure
                        )

                        VStack(alignment: .leading, spacing: 4) {
                            Text("Approval tools")
                                .font(.subheadline.weight(.semibold))
                                .foregroundStyle(SecurePalette.textPrimary)
                            Text("Open QR and root authorization controls")
                                .font(.footnote)
                                .foregroundStyle(SecurePalette.textSecondary)
                                .lineLimit(2)
                        }

                        Spacer(minLength: 12)

                        SecureMetricBadge(
                            title: "Mode",
                            value: "QR",
                            systemImage: "qrcode.viewfinder",
                            accent: SecurePalette.accentSky
                        )
                    }
                    .padding(.vertical, 3)
                }
                .buttonStyle(.plain)
            }
        }
        .listStyle(.insetGrouped)
        .secureInsetGroupedList()
        .navigationTitle("Security Center")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
        .tint(SecurePalette.accent)
    }

    private var overviewCard: some View {
        HStack(alignment: .center, spacing: 14) {
            ZStack(alignment: .bottomTrailing) {
                SecureIdentityAvatar(
                    title: "Transport",
                    seed: "security-transport",
                    kind: .system,
                    size: 54,
                    presence: clientStore.remoteOK ? .secure : .review,
                    prominent: true
                )
                SecureIdentityAvatar(
                    title: currentDeviceLabel,
                    seed: currentDeviceLabel,
                    kind: .device,
                    size: 28
                )
                .offset(x: 5, y: 5)
            }

            VStack(alignment: .leading, spacing: 8) {
                Text("Transport trust and root approval stay separated, but visible in one place.")
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)

                HStack(spacing: 8) {
                    SecureMetricBadge(
                        title: "Transport",
                        value: transportTitle,
                        systemImage: transportIcon,
                        accent: transportTone.accent
                    )
                    SecureMetricBadge(
                        title: "Root auth",
                        value: rootAuthStatusLabel,
                        systemImage: "key.horizontal.fill",
                        accent: rootAuthStore.publicKeyHex.isEmpty ? SecurePalette.warning : SecurePalette.success
                    )
                }
            }

            Spacer(minLength: 0)

            SecureCircularIconButton(
                systemImage: "arrow.clockwise",
                accessibilityLabel: "Refresh security state",
                iconSize: 12,
                buttonSize: 30
            ) {
                clientStore.refreshNow()
            }
        }
        .secureNavigationGlass(cornerRadius: 24)
    }

    private var currentDeviceCard: some View {
        HStack(spacing: 12) {
            SecureIdentityAvatar(
                title: currentDeviceLabel,
                seed: currentDeviceLabel,
                kind: .device,
                size: 52,
                presence: clientStore.remoteOK ? .secure : .review,
                prominent: true
            )

            VStack(alignment: .leading, spacing: 6) {
                Text(currentDeviceLabel)
                    .font(.headline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .lineLimit(1)

                Text(clientStore.remoteOK ? "Healthy transport session on this device." : "Transport state needs review on this device.")
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(2)

                HStack(spacing: 8) {
                    SecureMetricBadge(
                        title: "Role",
                        value: "Current",
                        systemImage: "iphone.gen3",
                        accent: SecurePalette.accentSky
                    )
                    SecureMetricBadge(
                        title: "State",
                        value: clientStore.remoteOK ? "Healthy" : "Review",
                        systemImage: clientStore.remoteOK ? "checkmark.shield.fill" : "shield.lefthalf.filled",
                        accent: clientStore.remoteOK ? SecurePalette.success : SecurePalette.warning
                    )
                }
            }

            Spacer(minLength: 0)
        }
        .secureCard(padding: 16)
    }

    private func shortHex(_ value: String) -> String {
        guard value.count > 20 else {
            return value
        }
        return "\(value.prefix(10))...\(value.suffix(8))"
    }
}

private struct SecurityMetricRow: View {
    let title: String
    let value: String
    let systemImage: String
    let accent: Color
    let detail: String
    var monospaced: Bool = false

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: systemImage)
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(accent)
                .frame(width: 22, height: 22)
                .background(
                    Circle()
                        .fill(accent.opacity(0.12))
                )

            VStack(alignment: .leading, spacing: 3) {
                Text(title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                if !detail.isEmpty {
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                        .lineLimit(2)
                }
            }

            Spacer(minLength: 12)

            Text(value)
                .font(monospaced
                      ? .system(.footnote, design: .monospaced).weight(.semibold)
                      : .footnote.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
                .multilineTextAlignment(.trailing)
                .lineLimit(2)
                .minimumScaleFactor(0.82)
        }
        .padding(.vertical, 3)
    }
}

private struct SecurityLinkedDeviceRow: View {
    let summary: String

    private var parts: (name: String, state: String, detail: String) {
        let tokens = summary
            .split(separator: "·", omittingEmptySubsequences: false)
            .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
        let name = tokens.indices.contains(0) ? tokens[0] : summary
        let state = tokens.indices.contains(1) ? tokens[1] : "Linked"
        let detail = tokens.indices.contains(2) ? tokens[2] : "Trusted device"
        return (name, state, detail)
    }

    var body: some View {
        HStack(spacing: 12) {
            SecureIdentityAvatar(
                title: parts.name,
                seed: summary,
                kind: .device,
                size: 44,
                presence: .active
            )

            VStack(alignment: .leading, spacing: 4) {
                Text(parts.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                Text(parts.detail)
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(2)
            }

            Spacer(minLength: 0)

            Text(parts.state.uppercased())
                .font(.caption2.weight(.bold))
                .tracking(0.8)
                .foregroundStyle(SecurePalette.accent)
                .padding(.horizontal, 8)
                .padding(.vertical, 6)
                .background(
                    Capsule()
                        .fill(SecurePalette.accentSoft)
                )
        }
        .padding(.vertical, 3)
    }
}
