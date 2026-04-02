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

    private var rootAuthStatusLabel: String {
        rootAuthStore.publicKeyHex.isEmpty ? "Not configured" : "Configured"
    }

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 12,
                                   verticalPadding: 8,
                                   showsIndicators: false) {
            VStack(alignment: .leading, spacing: 10) {
                summaryStrip

                sectionLabel("Devices")
                securityGroup {
                    securityInfoRow(
                        title: "Current device",
                        detail: currentDeviceLabel,
                        systemImage: "iphone.gen3",
                        trailing: "This iPhone",
                        monospaced: true
                    )

                    securityDivider()

                    if linkedDeviceSummaries.isEmpty {
                        securityInfoRow(
                            title: "Linked devices",
                            detail: "No additional trusted devices are currently active.",
                            systemImage: "macbook.and.iphone",
                            trailing: "0",
                            lineLimit: 2
                        )
                    } else {
                        ForEach(Array(linkedDeviceSummaries.prefix(3).enumerated()), id: \.offset) { index, summary in
                            securityInfoRow(
                                title: index == 0 ? "Linked devices" : " ",
                                detail: summary,
                                systemImage: "ipad.and.iphone",
                                trailing: "Trusted",
                                lineLimit: 2
                            )

                            if index < min(linkedDeviceSummaries.count, 3) - 1 {
                                securityDivider()
                            }
                        }
                    }
                }

                sectionLabel("Trust & Transport")
                securityGroup {
                    securityInfoRow(
                        title: "Secure transport",
                        detail: transportDetail,
                        systemImage: transportIcon,
                        trailing: transportTone == .success ? "Healthy" : "Review"
                    )
                    securityDivider()
                    securityInfoRow(
                        title: "Gateway",
                        detail: "\(clientStore.serverHost):\(clientStore.serverPort)",
                        systemImage: "server.rack",
                        trailing: clientStore.useTLS ? "TLS" : "TCP"
                    )
                    securityDivider()
                    securityInfoRow(
                        title: "Trusted state",
                        detail: clientStore.remoteOK
                            ? "Pinned session and device trust look normal."
                            : "Waiting for transport validation from the secure gateway.",
                        systemImage: "checkmark.shield",
                        trailing: clientStore.remoteOK ? "Normal" : "Checking"
                    )
                }

                sectionLabel("Root Authorization")
                securityGroup {
                    securityInfoRow(
                        title: "Approval tools",
                        detail: rootAuthStore.publicKeyHex.isEmpty
                            ? "Set up root approval on this iPhone before approving linked-device logins."
                            : "Approval tools are ready when a linked device needs verification.",
                        systemImage: "key.horizontal",
                        trailing: rootAuthStatusLabel
                    )
                    securityDivider()
                    NavigationLink(destination: ContentView(store: rootAuthStore,
                                                           embeddedTitle: "Root authorization tools")) {
                        HStack(spacing: 12) {
                            Image(systemName: "qrcode.viewfinder")
                                .font(.system(size: 14, weight: .semibold))
                                .foregroundStyle(SecurePalette.accent)
                                .frame(width: 22, height: 22)

                            VStack(alignment: .leading, spacing: 3) {
                                Text("Open approval tools")
                                    .font(.subheadline.weight(.semibold))
                                    .foregroundStyle(SecurePalette.textPrimary)
                                Text("Scan requests, generate approval strings, and manage the signer on a deeper page.")
                                    .font(.footnote)
                                    .foregroundStyle(SecurePalette.textSecondary)
                            }

                            Spacer(minLength: 12)

                            Image(systemName: "chevron.right")
                                .font(.footnote.weight(.semibold))
                                .foregroundStyle(SecurePalette.textMuted)
                        }
                        .padding(.horizontal, 14)
                        .frame(minHeight: 54)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
            }
        }
        .navigationTitle("Security")
        .navigationBarTitleDisplayMode(NavigationBarItem.TitleDisplayMode.inline)
        .tint(SecurePalette.accent)
    }

    private var summaryStrip: some View {
        HStack(spacing: 8) {
            Image(systemName: transportIcon)
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(transportTone.accent)
                .frame(width: 18, height: 18)
                .background(
                    Circle()
                        .fill(transportTone.fill)
                )

            Text(clientStore.remoteOK ? "No security action needed" : transportTitle)
                .font(.footnote.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
                .lineLimit(1)

            Spacer(minLength: 8)

            SecureCircularIconButton(
                systemImage: "arrow.clockwise",
                accessibilityLabel: "Refresh security state",
                iconSize: 11,
                buttonSize: 28
            ) {
                clientStore.refreshNow()
            }
        }
        .padding(.horizontal, 10)
        .frame(height: 36)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(SecurePalette.surface.opacity(0.94))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }

    private func sectionLabel(_ title: String) -> some View {
        Text(title.uppercased())
            .font(.caption.weight(.semibold))
            .tracking(1.0)
            .foregroundStyle(SecurePalette.textMuted)
            .padding(.horizontal, 4)
    }

    private func securityGroup<Content: View>(@ViewBuilder content: () -> Content) -> some View {
        VStack(spacing: 0) {
            content()
        }
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(SecurePalette.surface.opacity(0.96))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }

    private func securityDivider() -> some View {
        Divider()
            .overlay(SecurePalette.border)
            .padding(.leading, 54)
    }

    private func securityInfoRow(title: String,
                                 detail: String,
                                 systemImage: String,
                                 trailing: String,
                                 monospaced: Bool = false,
                                 lineLimit: Int = 2) -> some View {
        HStack(spacing: 12) {
            Image(systemName: systemImage)
                .font(.system(size: 14, weight: .semibold))
                .foregroundStyle(SecurePalette.accent)
                .frame(width: 22, height: 22)

            VStack(alignment: .leading, spacing: 3) {
                Text(title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                Text(detail)
                    .font(monospaced ? .system(.footnote, design: .monospaced) : .footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(lineLimit)
            }

            Spacer(minLength: 12)

            Text(trailing)
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textMuted)
                .multilineTextAlignment(.trailing)
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 54)
    }
}
