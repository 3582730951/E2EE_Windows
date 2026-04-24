import Foundation
import SwiftUI
import UIKit

struct SecureFullscreenScrollPage<Content: View>: View {
    let horizontalPadding: CGFloat
    let verticalPadding: CGFloat
    let showsIndicators: Bool
    let content: () -> Content

    init(horizontalPadding: CGFloat,
         verticalPadding: CGFloat,
         showsIndicators: Bool,
         @ViewBuilder content: @escaping () -> Content) {
        self.horizontalPadding = horizontalPadding
        self.verticalPadding = verticalPadding
        self.showsIndicators = showsIndicators
        self.content = content
    }

    var body: some View {
        GeometryReader { proxy in
            ZStack(alignment: .top) {
                SecureSceneBackground()

                ScrollView(showsIndicators: showsIndicators) {
                    content()
                        .frame(maxWidth: .infinity, alignment: .top)
                        .frame(minHeight: max(proxy.size.height - (verticalPadding * 2), 0), alignment: .top)
                        .padding(.horizontal, horizontalPadding)
                        .padding(.vertical, verticalPadding)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(SecurePalette.backgroundBottom)
    }
}

private struct SecureInsetGroupedListPage<Content: View>: View {
    let content: () -> Content

    init(@ViewBuilder content: @escaping () -> Content) {
        self.content = content
    }

    var body: some View {
        List {
            content()
        }
        .listStyle(.insetGrouped)
        .environment(\.defaultMinListRowHeight, 52)
        .secureInsetGroupedList()
    }
}

private struct SecureNavigationRow<Destination: View>: View {
    let title: String
    let detail: String
    var systemImage: String? = nil
    var identityTitle: String? = nil
    var identitySeed: String? = nil
    var identityKind: SecureIdentityKind = .person
    var identityPresence: SecurePresenceState = .none
    var badge: String? = nil
    let destination: Destination

    private var iconAccent: Color {
        switch systemImage ?? "" {
        case "shield.fill", "lock.shield.fill":
            return SecurePalette.success
        case "iphone.gen3", "ipad.landscape":
            return SecurePalette.accentSky
        case "paintpalette.fill", "textformat":
            return SecurePalette.accentLavender
        default:
            return SecurePalette.accent
        }
    }

    var body: some View {
        NavigationLink(destination: destination) {
            HStack(spacing: 10) {
                if let identityTitle {
                    SecureIdentityAvatar(
                        title: identityTitle,
                        seed: identitySeed ?? identityTitle,
                        kind: identityKind,
                        size: 36,
                        presence: identityPresence
                    )
                } else if let systemImage {
                    RoundedRectangle(cornerRadius: 9, style: .continuous)
                        .fill(iconAccent)
                        .frame(width: 30, height: 30)
                        .overlay(
                            Image(systemName: systemImage)
                                .font(.system(size: 13, weight: .semibold))
                                .foregroundStyle(Color.white)
                        )
                }

                VStack(alignment: .leading, spacing: 4) {
                    HStack(spacing: 8) {
                        Text(title)
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(SecurePalette.textPrimary)
                        if let badge, !badge.isEmpty {
                            Text(badge)
                                .font(.caption2.weight(.semibold))
                                .foregroundStyle(SecurePalette.accent)
                                .padding(.horizontal, 7)
                                .padding(.vertical, 3)
                                .background(
                                    Capsule()
                                        .fill(SecurePalette.accentSoft)
                                )
                        }
                    }
                    if !detail.isEmpty {
                        Text(detail)
                            .font(.footnote)
                            .foregroundStyle(SecurePalette.textSecondary)
                            .lineLimit(1)
                    }
                }

                Spacer(minLength: 12)

                Image(systemName: "chevron.right")
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(SecurePalette.textMuted)
            }
            .padding(.horizontal, 14)
            .frame(minHeight: 54, alignment: .center)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

private func clientMediaKind(for text: String) -> SecureMediaKind {
    SecureMediaKind.detect(in: text)
}

private func sanitizedPreviewText(_ text: String) -> String {
    let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
    guard !trimmed.isEmpty else {
        return "No preview"
    }
    let prefixes = ["[File]", "[Photo]", "[Voice]"]
    for prefix in prefixes where trimmed.hasPrefix(prefix) {
        return trimmed.replacingOccurrences(of: prefix, with: "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }
    if trimmed.contains("https://") || trimmed.contains("http://") {
        return "Shared secure link"
    }
    return trimmed
}

private func conversationPresence(isMuted: Bool, isSelected: Bool) -> SecurePresenceState {
    if isMuted {
        return .muted
    }
    return isSelected ? .secure : .active
}

private func deviceSummaryComponents(_ summary: String) -> (name: String, state: String, detail: String) {
    let parts = summary
        .split(separator: "·", omittingEmptySubsequences: false)
        .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
    let name = parts.indices.contains(0) ? parts[0] : summary
    let state = parts.indices.contains(1) ? parts[1] : "Linked"
    let detail = parts.indices.contains(2) ? parts[2] : ""
    return (name, state, detail)
}

private struct ClientOverviewHero: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var statusTone: SecureBannerTone {
        if !store.lastError.isEmpty && !store.remoteOK {
            return .danger
        }
        return store.remoteOK ? .success : .warning
    }

    var body: some View {
        HStack(alignment: .top, spacing: 14) {
                ZStack(alignment: .bottomTrailing) {
                    SecureIdentityAvatar(
                        title: store.username.isEmpty ? "Secure" : store.username,
                        seed: store.username.isEmpty ? "secure-user" : store.username,
                        kind: .person,
                        size: 46,
                        presence: store.remoteOK ? .secure : .review,
                        prominent: true
                    )

                    SecureIdentityAvatar(
                        title: store.deviceDisplayID.isEmpty ? "Phone" : store.deviceDisplayID,
                        seed: store.deviceDisplayID.isEmpty ? "device" : store.deviceDisplayID,
                        kind: .device,
                        size: 24,
                        presence: .none
                    )
                .offset(x: 4, y: 4)
            }

            VStack(alignment: .center, spacing: 8) {
                HStack(spacing: 8) {
                    SecureMetricBadge(
                        title: "Session",
                        value: store.remoteOK ? "Secure" : "Review",
                        systemImage: statusTone == .success ? "checkmark.shield.fill" : "shield.lefthalf.filled",
                        accent: statusTone.accent
                    )

                    if !store.deviceDisplayID.isEmpty {
                        SecureMetricBadge(
                            title: "Device",
                            value: store.deviceDisplayID,
                            systemImage: "iphone.gen3",
                            accent: SecurePalette.accentSky
                        )
                    }
                }
                .frame(maxWidth: .infinity, alignment: .center)

                Text(store.isLoggedIn ? "Chats" : "Sign in")
                    .font(.title3.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .frame(maxWidth: .infinity, alignment: .center)

                Text(store.remoteOK ? "Secure" : "Review")
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .frame(maxWidth: .infinity, alignment: .center)
            }
        }
    }
}

private struct ClientDeviceModule: View {
    let summary: String
    var current: Bool = false

    private var parts: (name: String, state: String, detail: String) {
        deviceSummaryComponents(summary)
    }

    var body: some View {
        HStack(spacing: 12) {
            SecureIdentityAvatar(
                title: parts.name,
                seed: summary,
                kind: .device,
                size: 42,
                presence: current ? .secure : .active
            )

            VStack(alignment: .leading, spacing: 4) {
                Text(parts.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .lineLimit(1)

                Text(parts.detail.isEmpty ? "Trusted device" : parts.detail)
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(1)
            }

            Spacer(minLength: 12)

            Text(parts.state)
                .font(.caption2.weight(.semibold))
                .foregroundStyle(current ? SecurePalette.success : SecurePalette.accent)
                .padding(.horizontal, 8)
                .padding(.vertical, 6)
                .background(
                    Capsule()
                        .fill((current ? SecurePalette.success : SecurePalette.accent).opacity(0.12))
                )
        }
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(SecurePalette.surfaceRaised)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }
}

struct ClientStatusCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var statusDetail: String {
        if !store.lastError.isEmpty {
            return store.lastError
        }
        return store.statusText
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .center, spacing: 14) {
                ZStack(alignment: .bottomTrailing) {
                    SecureIdentityAvatar(
                        title: store.username.isEmpty ? "Secure" : store.username,
                        seed: store.username.isEmpty ? "secure-user" : store.username,
                        kind: .person,
                        size: 42,
                        presence: store.remoteOK ? .secure : .review,
                        prominent: true
                    )

                    SecureIdentityAvatar(
                        title: store.deviceDisplayID.isEmpty ? "Phone" : store.deviceDisplayID,
                        seed: store.deviceDisplayID.isEmpty ? "device" : store.deviceDisplayID,
                        kind: .device,
                        size: 22,
                        presence: .none
                    )
                    .offset(x: 4, y: 4)
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text(store.username.isEmpty ? "MI E2EE" : store.username)
                        .font(.headline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                }

                Spacer(minLength: 0)

                ClientInlineSecurityStatus(store: store)

                SecureCircularIconButton(
                    systemImage: "arrow.clockwise",
                    accessibilityLabel: "Refresh session state",
                    iconSize: 12,
                    buttonSize: 30
                ) {
                    store.refreshNow()
                }
            }

            if !store.lastError.isEmpty && !store.remoteOK {
                Text(statusDetail)
                    .font(.caption)
                    .foregroundStyle(SecurePalette.danger)
                    .lineLimit(2)
            }
        }
        .secureCard(padding: 14)
    }
}

struct ClientLoginCard: View {
    @ObservedObject var store: ClientWorkspaceStore
    @State private var showsAdvancedApproval = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SecureSectionHeader(
                eyebrow: "Trusted Access",
                title: "Sign in",
                detail: "Use your primary account, then link extra devices from Security Center."
            )

            if !store.lastError.isEmpty {
                ClientSecuritySummaryCard(store: store)
            }

            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 10) {
                    Image(systemName: "person.crop.circle")
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(SecurePalette.textMuted)
                    TextField("Phone or email", text: $store.username)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled(true)
                }
                .secureInput()
                HStack(spacing: 10) {
                    Image(systemName: "lock.fill")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(SecurePalette.textMuted)
                    SecureField("Password", text: $store.password)
                }
                .secureInput()
            }
            .padding(12)
            .secureInsetGroupedSection(cornerRadius: 18)

            HStack {
                Spacer(minLength: 0)
                SecureCircularIconButton(
                    systemImage: "arrow.right",
                    accessibilityLabel: "Sign in",
                    iconSize: 16,
                    buttonSize: 50,
                    foreground: Color.white,
                    fill: SecurePalette.accent,
                    stroke: SecurePalette.accent
                ) {
                    store.signIn()
                }
                Spacer(minLength: 0)
            }

            HStack(spacing: 12) {
                SecureCircularIconButton(
                    systemImage: showsAdvancedApproval ? "ellipsis.circle.fill" : "ellipsis.circle",
                    accessibilityLabel: showsAdvancedApproval ? "Hide advanced" : "Show advanced",
                    iconSize: 14,
                    buttonSize: 40,
                    foreground: SecurePalette.textPrimary
                ) {
                    withAnimation(.easeInOut(duration: 0.18)) {
                        showsAdvancedApproval.toggle()
                    }
                }

                Spacer(minLength: 0)

                SecureCircularIconButton(
                    systemImage: "qrcode",
                    accessibilityLabel: "Show QR sign in",
                    iconSize: 15,
                    buttonSize: 40,
                    foreground: SecurePalette.textPrimary
                ) {
                }

                SecureCircularIconButton(
                    systemImage: "person.badge.plus",
                    accessibilityLabel: "Create account",
                    iconSize: 14,
                    buttonSize: 40,
                    foreground: SecurePalette.textPrimary
                ) {
                    store.registerAccount()
                }
            }
            .padding(.horizontal, 2)

            if showsAdvancedApproval {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Advanced")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(SecurePalette.textMuted)

                    HStack(spacing: 10) {
                        Image(systemName: "server.rack")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(SecurePalette.textMuted)
                        TextField("Server", text: $store.serverHost)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled(true)
                    }
                    .secureInput()

                    HStack(spacing: 10) {
                        HStack(spacing: 10) {
                            Image(systemName: "number.square")
                                .font(.system(size: 13, weight: .semibold))
                                .foregroundStyle(SecurePalette.textMuted)
                            TextField("Port", text: $store.serverPort)
                                .keyboardType(.numberPad)
                        }
                        .secureInput()

                        Toggle(isOn: $store.useTLS) {
                            Image(systemName: "lock.shield")
                                .font(.system(size: 13, weight: .semibold))
                                .foregroundStyle(SecurePalette.textPrimary)
                        }
                        .tint(SecurePalette.accent)
                        .padding(.horizontal, 12)
                        .frame(height: 40)
                        .background(
                            RoundedRectangle(cornerRadius: 14, style: .continuous)
                                .fill(SecurePalette.surfaceRaised)
                        )
                        .overlay(
                            RoundedRectangle(cornerRadius: 14, style: .continuous)
                                .stroke(SecurePalette.borderStrong, lineWidth: 1)
                        )
                    }

                    HStack(spacing: 10) {
                        Image(systemName: "key")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(SecurePalette.textMuted)
                        TextField("Approval code", text: $store.rootCode)
                            .textInputAutocapitalization(.never)
                            .autocorrectionDisabled(true)
                    }
                    .secureInput()
                }
                .padding(12)
                .secureInsetGroupedSection(cornerRadius: 18)
            }
        }
        .secureNavigationGlass(cornerRadius: 22)
    }
}

struct ClientAuthShellView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 14,
                                   verticalPadding: 14,
                                   showsIndicators: false) {
            VStack(spacing: 8) {
                ClientAuthHeader()
                ClientLoginCard(store: store)
            }
            .frame(maxWidth: 400, alignment: .top)
            .frame(maxWidth: .infinity, alignment: .top)
        }
        .navigationTitle("Sign in")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
    }
}

private struct ClientAuthHeader: View {
    var body: some View {
        VStack(spacing: 10) {
            HStack {
                Spacer(minLength: 0)
                SecureIdentityAvatar(
                    title: "MI E2EE",
                    seed: "mi-e2ee-shell",
                    kind: .system,
                    size: 52,
                    presence: .secure,
                    prominent: true
                )
                Spacer(minLength: 0)
            }

            Text("Telegram rhythm, native Apple structure, device-first trust.")
                .font(.footnote)
                .foregroundStyle(SecurePalette.textSecondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
    }
}

private struct ClientSecuritySummaryCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var statusText: String {
        if !store.lastError.isEmpty {
            return store.lastError
        }
        return store.remoteOK ? "Ready" : "Checking"
    }

    var body: some View {
        HStack(spacing: 8) {
            SecureIdentityAvatar(
                title: store.remoteOK ? "Secure" : "Review",
                seed: store.remoteOK ? "summary-secure" : "summary-review",
                kind: .system,
                size: 24,
                presence: store.remoteOK ? .secure : .review
            )

            Text(store.remoteOK ? "Secure" : "Checking")
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)
                .lineLimit(1)

            Spacer(minLength: 8)

            Text(statusText)
                .font(.caption2)
                .foregroundStyle(SecurePalette.textSecondary)
                .lineLimit(1)
        }
        .padding(.horizontal, 9)
        .frame(height: 30)
        .secureNavigationGlass(cornerRadius: 12)
    }
}

private struct ClientInlineSecurityStatus: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var tint: Color {
        if !store.lastError.isEmpty && !store.remoteOK {
            return SecurePalette.danger
        }
        return store.remoteOK ? SecurePalette.success : SecurePalette.warning
    }

    var body: some View {
        HStack(spacing: 0) {
            SecureIdentityAvatar(
                title: store.remoteOK ? "Secure" : "Review",
                seed: store.remoteOK ? "inline-secure" : "inline-review",
                kind: .system,
                size: 18,
                presence: store.remoteOK ? .secure : .review
            )
        }
        .padding(.horizontal, 6)
        .frame(height: 24)
        .background(
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .fill(SecurePalette.surface.opacity(0.92))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }
}

private struct ClientConversationRow: View {
    let conversation: ClientConversation
    let preview: String
    let timestampMS: UInt64
    let isSelected: Bool
    let isPinned: Bool
    let isMuted: Bool
    let isUnread: Bool

    private static let timeFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        return formatter
    }()

    private static let dayFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "EEE"
        return formatter
    }()

    private var timestampLabel: String {
        guard timestampMS > 0 else {
            return ""
        }
        let date = Date(timeIntervalSince1970: TimeInterval(timestampMS) / 1000.0)
        if Calendar.current.isDateInToday(date) {
            return Self.timeFormatter.string(from: date)
        }
        if Calendar.current.isDateInYesterday(date) {
            return "Yesterday"
        }
        return Self.dayFormatter.string(from: date)
    }

    private var mediaKind: SecureMediaKind {
        clientMediaKind(for: preview)
    }

    private var cleanedPreview: String {
        sanitizedPreviewText(preview)
    }

    var body: some View {
        HStack(spacing: 10) {
            SecureIdentityAvatar(
                title: conversation.title,
                seed: conversation.id,
                kind: conversation.isGroup ? .group : .person,
                size: 44,
                presence: conversationPresence(isMuted: isMuted, isSelected: isSelected)
            )

            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(conversation.title)
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                        .lineLimit(1)

                    Spacer(minLength: 8)
                }

                HStack(spacing: 6) {
                    if mediaKind != .none {
                        HStack(spacing: 4) {
                            SecureMediaHintPill(kind: mediaKind)
                            Text(mediaKind.label)
                                .font(.caption2.weight(.semibold))
                                .foregroundStyle(mediaKind.accent)
                        }
                    }

                    if isPinned {
                        Image(systemName: "pin.fill")
                            .font(.system(size: 10, weight: .semibold))
                            .foregroundStyle(SecurePalette.textMuted)
                    }

                    if isMuted {
                        Image(systemName: "bell.slash.fill")
                            .font(.system(size: 10, weight: .semibold))
                            .foregroundStyle(SecurePalette.textMuted)
                    }

                    Text(cleanedPreview)
                        .font(.system(size: 13))
                        .foregroundStyle(SecurePalette.textSecondary)
                        .lineLimit(1)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            VStack(alignment: .trailing, spacing: 6) {
                if !timestampLabel.isEmpty {
                    Text(timestampLabel)
                        .font(.system(size: 12, weight: .medium))
                        .foregroundStyle(SecurePalette.textMuted)
                }

                if isUnread {
                    Circle()
                        .fill(SecurePalette.accent)
                        .frame(width: 10, height: 10)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                        .background(
                            Capsule()
                                .fill(SecurePalette.accent.opacity(0.14))
                        )
                } else {
                    Color.clear
                        .frame(width: 26, height: 16)
                }
            }
        }
        .padding(.horizontal, 4)
        .frame(minHeight: 70)
        .background(
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(isSelected ? SecurePalette.selectedRow : Color.clear)
        )
        .contentShape(Rectangle())
    }
}

private struct ClientConversationListCard: View {
    @ObservedObject var store: ClientWorkspaceStore
    @State private var query: String = ""
    @State private var pinnedConversationIDs: Set<String> = []
    @State private var mutedConversationIDs: Set<String> = []
    @State private var unreadConversationIDs: Set<String> = []
    @State private var hiddenConversationIDs: Set<String> = []

    private var orderedConversations: [ClientConversation] {
        store.conversations
            .filter { !hiddenConversationIDs.contains($0.id) }
            .sorted { lhs, rhs in
            let lhsPinned = pinnedConversationIDs.contains(lhs.id)
            let rhsPinned = pinnedConversationIDs.contains(rhs.id)
            if lhsPinned != rhsPinned {
                return lhsPinned && !rhsPinned
            }
            let lhsTime = store.latestMessage(for: lhs.id)?.timestampMS ?? 0
            let rhsTime = store.latestMessage(for: rhs.id)?.timestampMS ?? 0
            if lhsTime == rhsTime {
                return lhs.title.localizedCaseInsensitiveCompare(rhs.title) == .orderedAscending
            }
            return lhsTime > rhsTime
        }
    }

    private var filteredConversations: [ClientConversation] {
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else {
            return orderedConversations
        }
        return orderedConversations.filter { conversation in
            if conversation.title.localizedCaseInsensitiveContains(trimmed) {
                return true
            }
            let preview = store.latestMessage(for: conversation.id)?.text ?? conversation.subtitle
            return preview.localizedCaseInsensitiveContains(trimmed)
        }
    }

    private func togglePinned(_ conversationID: String) {
        if pinnedConversationIDs.contains(conversationID) {
            pinnedConversationIDs.remove(conversationID)
        } else {
            pinnedConversationIDs.insert(conversationID)
        }
    }

    private func toggleMuted(_ conversationID: String) {
        if mutedConversationIDs.contains(conversationID) {
            mutedConversationIDs.remove(conversationID)
        } else {
            mutedConversationIDs.insert(conversationID)
        }
    }

    private func toggleUnread(_ conversationID: String) {
        if unreadConversationIDs.contains(conversationID) {
            unreadConversationIDs.remove(conversationID)
        } else {
            unreadConversationIDs.insert(conversationID)
        }
    }

    private func copyConversationTitle(_ title: String) {
        UIPasteboard.general.string = title
    }

    private func archiveConversation(_ conversationID: String) {
        hiddenConversationIDs.insert(conversationID)
        pinnedConversationIDs.remove(conversationID)
        mutedConversationIDs.remove(conversationID)
        unreadConversationIDs.remove(conversationID)
    }

    var body: some View {
        let rows = filteredConversations

        List {
            Section {
                ClientStatusCard(store: store)
                    .listRowInsets(EdgeInsets(top: 4, leading: 0, bottom: 6, trailing: 0))
                    .listRowSeparator(.hidden)
                    .listRowBackground(Color.clear)
            }

            if rows.isEmpty {
                Section {
                    VStack(spacing: 12) {
                        SecureEmptyStateIllustration(systemImage: "bubble.left.and.bubble.right.fill")
                        SecureStatusBanner(
                            title: store.conversations.isEmpty ? "No chats yet" : "No matching chats",
                            detail: store.conversations.isEmpty
                                ? "Secure conversations appear here once contacts or incoming events are available."
                                : "Try a different keyword.",
                            tone: .neutral,
                            systemImage: "bubble.left.and.text.bubble.right"
                        )
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 18)
                }
            } else {
                Section {
                    ForEach(rows) { conversation in
                        let preview = store.latestMessage(for: conversation.id)?.text ?? conversation.subtitle
                        let timestamp = store.latestMessage(for: conversation.id)?.timestampMS ?? 0
                        let isPinned = pinnedConversationIDs.contains(conversation.id)
                        let isMuted = mutedConversationIDs.contains(conversation.id)
                        let isUnread = unreadConversationIDs.contains(conversation.id)

                        NavigationLink {
                            ClientConversationDetailView(
                                store: store,
                                conversation: conversation,
                                sourceTitle: "Chats"
                            )
                            .onAppear {
                                store.selectConversation(conversation.id)
                                unreadConversationIDs.remove(conversation.id)
                            }
                        } label: {
                            ClientConversationRow(
                                conversation: conversation,
                                preview: preview,
                                timestampMS: timestamp,
                                isSelected: store.selectedConversationID == conversation.id,
                                isPinned: isPinned,
                                isMuted: isMuted,
                                isUnread: isUnread
                            )
                        }
                        .buttonStyle(.plain)
                        .listRowInsets(EdgeInsets(top: 2, leading: 2, bottom: 2, trailing: 2))
                        .listRowSeparator(.hidden)
                        .swipeActions(edge: .leading, allowsFullSwipe: false) {
                            Button {
                                togglePinned(conversation.id)
                            } label: {
                                Label(isPinned ? "Unpin" : "Pin", systemImage: isPinned ? "pin.slash" : "pin.fill")
                            }
                            .tint(.orange)

                            Button {
                                toggleUnread(conversation.id)
                            } label: {
                                Label(isUnread ? "Read" : "Unread", systemImage: isUnread ? "envelope.open.fill" : "envelope.badge.fill")
                            }
                            .tint(SecurePalette.accent)
                        }
                        .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                            Button {
                                toggleMuted(conversation.id)
                            } label: {
                                Label(isMuted ? "Unmute" : "Mute", systemImage: isMuted ? "bell.fill" : "bell.slash.fill")
                            }
                            .tint(.gray)

                            Button(role: .destructive) {
                                archiveConversation(conversation.id)
                            } label: {
                                Label("Delete", systemImage: "trash.fill")
                            }
                        }
                        .contextMenu {
                            Button {
                                store.selectConversation(conversation.id)
                            } label: {
                                Label("Open chat", systemImage: "bubble.left.and.bubble.right.fill")
                            }

                            Button {
                                togglePinned(conversation.id)
                            } label: {
                                Label(isPinned ? "Unpin conversation" : "Pin conversation",
                                      systemImage: isPinned ? "pin.slash" : "pin.fill")
                            }

                            Button {
                                toggleUnread(conversation.id)
                            } label: {
                                Label(isUnread ? "Mark as read" : "Mark as unread",
                                      systemImage: isUnread ? "envelope.open.fill" : "envelope.badge.fill")
                            }

                            Button {
                                toggleMuted(conversation.id)
                            } label: {
                                Label(isMuted ? "Unmute notifications" : "Mute notifications",
                                      systemImage: isMuted ? "bell.fill" : "bell.slash.fill")
                            }

                            Divider()

                            Button {
                                copyConversationTitle(conversation.title)
                            } label: {
                                Label("Copy chat name", systemImage: "doc.on.doc")
                            }

                            Button(role: .destructive) {
                                archiveConversation(conversation.id)
                            } label: {
                                Label("Delete chat", systemImage: "trash.fill")
                            }
                        }
                    }
                }
            }
        }
        .listStyle(.insetGrouped)
        .secureInsetGroupedList()
        .searchable(text: $query, placement: .navigationBarDrawer(displayMode: .always), prompt: "Search chats")
    }
}

private struct ClientConversationTitleView: View {
    let conversation: ClientConversation

    var body: some View {
        HStack(spacing: 8) {
            SecureIdentityAvatar(
                title: conversation.title,
                seed: conversation.id,
                kind: conversation.isGroup ? .group : .person,
                size: 28,
                presence: .secure
            )

            VStack(spacing: 0) {
                Text(conversation.title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .lineLimit(1)
            }
        }
        .frame(maxWidth: 180, alignment: .center)
        .accessibilityLabel(conversation.title)
    }
}

struct ClientConversationDetailView: View {
    @ObservedObject var store: ClientWorkspaceStore
    let conversation: ClientConversation
    var sourceTitle: String = "Chats"
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ZStack {
            SecureSceneBackground()
                .ignoresSafeArea()

            VStack(spacing: 8) {
                ClientDetailHeroCard(store: store, conversation: conversation)
                    .padding(.horizontal, 8)
                    .padding(.top, 4)

                ClientMessagesCard(store: store, showsThreadHeader: false)
                    .padding(.horizontal, 8)
                    .padding(.top, 0)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            }
        }
        .safeAreaInset(edge: .bottom) {
            ClientComposerCard(store: store, compact: true)
                .padding(.horizontal, 6)
                .padding(.top, 8)
                .padding(.bottom, 6)
                .secureFloatingComposer(cornerRadius: 28)
                .background(
                    Rectangle()
                        .fill(.ultraThinMaterial)
                        .ignoresSafeArea(edges: .bottom)
                )
        }
        .secureGlassNavigationBar()
        .navigationTitle("")
        .navigationBarTitleDisplayMode(.inline)
        .navigationBarBackButtonHidden(true)
        .toolbar(.hidden, for: .tabBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarBackground(SecurePalette.glassToolbarSurface, for: .navigationBar)
        .toolbar {
            ToolbarItem(placement: .navigationBarLeading) {
                Button(action: { dismiss() }) {
                    Image(systemName: "chevron.left")
                        .font(.footnote.weight(.semibold))
                }
                .accessibilityLabel(sourceTitle.isEmpty ? "Back" : "Back to \(sourceTitle.lowercased())")
            }

            ToolbarItem(placement: .principal) {
                ClientConversationTitleView(
                    conversation: conversation
                )
            }

            ToolbarItemGroup(placement: .navigationBarTrailing) {
                Button(action: {}) {
                    Image(systemName: "magnifyingglass")
                        .font(.system(size: 15, weight: .semibold))
                }
                .accessibilityLabel("Search conversation")

                Button(action: {}) {
                    Image(systemName: "phone")
                        .font(.system(size: 15, weight: .semibold))
                }
                .accessibilityLabel("Start secure call")

                Menu {
                    Button {
                        store.refreshNow()
                    } label: {
                        Label("Refresh conversation", systemImage: "arrow.clockwise")
                    }

                    Button {
                        UIPasteboard.general.string = conversation.title
                    } label: {
                        Label("Copy chat name", systemImage: "doc.on.doc")
                    }

                    if !store.draft.isEmpty {
                        Button(role: .destructive) {
                            store.draft = ""
                        } label: {
                            Label("Clear draft", systemImage: "xmark.circle")
                        }
                    }
                } label: {
                    Image(systemName: "ellipsis.circle")
                        .font(.system(size: 16, weight: .semibold))
                }
                .accessibilityLabel("Conversation actions")
            }
        }
        .onAppear {
            store.selectConversation(conversation.id)
            store.refreshNow()
        }
    }
}

private struct ClientDetailHeroCard: View {
    @ObservedObject var store: ClientWorkspaceStore
    let conversation: ClientConversation

    private var previewText: String {
        sanitizedPreviewText(store.latestMessage(for: conversation.id)?.text ?? conversation.subtitle)
    }

    private var mediaKind: SecureMediaKind {
        clientMediaKind(for: store.latestMessage(for: conversation.id)?.text ?? conversation.subtitle)
    }

    var body: some View {
        HStack(spacing: 12) {
            SecureIdentityAvatar(
                title: conversation.title,
                seed: conversation.id,
                kind: conversation.isGroup ? .group : .person,
                size: 48,
                presence: .secure,
                prominent: true
            )

            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 8) {
                    Text(conversation.title)
                        .font(.headline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                        .lineLimit(1)
                    SecureMediaHintPill(kind: mediaKind)
                }
                Text(previewText)
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(2)
                HStack(spacing: 8) {
                    Image(systemName: conversation.isGroup ? "person.3.fill" : "person.fill")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(SecurePalette.accentSky)
                        .frame(width: 22, height: 22)
                        .background(
                            Circle()
                                .fill(SecurePalette.accentSky.opacity(0.12))
                        )
                    Image(systemName: store.remoteOK ? "lock.shield.fill" : "shield.lefthalf.filled")
                        .font(.system(size: 12, weight: .semibold))
                        .foregroundStyle(store.remoteOK ? SecurePalette.success : SecurePalette.warning)
                        .frame(width: 22, height: 22)
                        .background(
                            Circle()
                                .fill((store.remoteOK ? SecurePalette.success : SecurePalette.warning).opacity(0.12))
                        )
                }
            }
            Spacer(minLength: 0)
        }
        .padding(14)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(SecurePalette.groupedSurfaceElevated)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
        .shadow(color: SecurePalette.flatCardShadow, radius: 10, x: 0, y: 2)
    }
}

struct ClientMessagesCard: View {
    @ObservedObject var store: ClientWorkspaceStore
    var showsThreadHeader: Bool = true

    private let formatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        return formatter
    }()

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if showsThreadHeader {
                HStack {
                    Text(store.currentConversationTitle)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textSecondary)
                    Spacer(minLength: 12)
                    Text("\(store.currentMessages.count) msgs")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(SecurePalette.textSecondary)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 6)
                        .background(
                            Capsule()
                                .fill(SecurePalette.surfaceRaised)
                        )
                }
            }

            if store.currentMessages.isEmpty {
                SecureStatusBanner(
                    title: "No encrypted messages yet",
                    detail: "Messages appear here as soon as this thread receives secure traffic.",
                    tone: .neutral,
                    systemImage: "text.bubble"
                )
            } else {
                ScrollView(showsIndicators: false) {
                    LazyVStack(spacing: 8) {
                        ForEach(store.currentMessages) { message in
                            let mediaKind = clientMediaKind(for: message.text)
                            let previewText = sanitizedPreviewText(message.text)
                            let bubbleStyle = SecureChatBubbleStyle(role: message.outgoing ? .outgoing : .incoming)
                            HStack {
                                if message.outgoing {
                                    Spacer(minLength: 40)
                                }

                                VStack(alignment: .leading, spacing: 3) {
                                    HStack(spacing: 8) {
                                        Text(message.outgoing ? "You" : message.sender)
                                            .font(.caption2.weight(.semibold))
                                            .foregroundStyle(bubbleStyle.secondaryText)
                                        Text(formatter.string(from: Date(timeIntervalSince1970: TimeInterval(message.timestampMS) / 1000.0)))
                                            .font(.caption2)
                                            .foregroundStyle(bubbleStyle.secondaryText)
                                    }

                                    if mediaKind != .none {
                                        SecureMediaPreviewCard(
                                            kind: mediaKind,
                                            title: mediaKind.label,
                                            detail: previewText,
                                            outgoing: message.outgoing
                                        )
                                    } else {
                                        Text(message.text)
                                            .font(.system(size: 14))
                                            .foregroundStyle(bubbleStyle.primaryText)
                                            .frame(maxWidth: .infinity, alignment: .leading)
                                    }
                                }
                                .frame(maxWidth: 252, alignment: .leading)
                                .secureChatBubble(
                                    bubbleStyle,
                                    cornerRadius: 18,
                                    horizontalPadding: 14,
                                    verticalPadding: 10
                                )
                                .contextMenu {
                                    Button {
                                        UIPasteboard.general.string = message.text
                                    } label: {
                                        Label("Copy message", systemImage: "doc.on.doc")
                                    }

                                    Button {
                                        UIPasteboard.general.string = message.sender
                                    } label: {
                                        Label("Copy sender", systemImage: "person.crop.circle")
                                    }
                                }

                                if !message.outgoing {
                                    Spacer(minLength: 40)
                                }
                            }
                        }
                    }
                    .frame(maxWidth: .infinity)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
}

struct ClientComposerCard: View {
    @ObservedObject var store: ClientWorkspaceStore
    var compact: Bool = false

    private var canSend: Bool {
        !store.selectedConversationID.isEmpty &&
            !store.draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }

    var body: some View {
        HStack(alignment: .center, spacing: compact ? 8 : 10) {
            Button(action: {}) {
                Image(systemName: "plus")
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .frame(width: compact ? 30 : 34, height: compact ? 30 : 34)
                    .background(
                        Circle()
                            .fill(SecurePalette.composerSurface)
                    )
                    .overlay(
                        Circle()
                            .stroke(SecurePalette.borderStrong, lineWidth: 1)
                    )
            }
            .buttonStyle(.plain)

            HStack(spacing: 8) {
                TextField("Message", text: $store.draft, axis: .vertical)
                    .lineLimit(1...1)
                    .frame(minHeight: compact ? 30 : 36)
                    .foregroundStyle(SecurePalette.textPrimary)

                Button(action: {}) {
                    Image(systemName: "face.smiling")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundStyle(SecurePalette.textMuted)
                }
                .buttonStyle(.plain)
            }
            .padding(.horizontal, compact ? 10 : 12)
            .padding(.vertical, compact ? 0 : 8)
            .background(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .fill(SecurePalette.composerSurface)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 18, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )

            Button(action: { if canSend { store.sendDraft() } }) {
                Image(systemName: canSend ? "paperplane.fill" : "mic.fill")
                    .font(.system(size: compact ? 14 : 16, weight: .semibold))
                    .foregroundStyle(Color.white)
                    .frame(width: compact ? 30 : 36, height: compact ? 30 : 36)
                    .background(
                        Circle()
                            .fill(canSend ? SecurePalette.outgoingBubble : SecurePalette.outgoingBubble.opacity(0.72))
                    )
            }
            .buttonStyle(.plain)
        }
        .frame(height: compact ? 42 : 50)
        .padding(.horizontal, compact ? 0 : 4)
    }
}

struct ClientDevicesCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "",
                title: "Devices",
                detail: ""
            )

            if store.deviceSummaries.isEmpty {
                VStack(spacing: 10) {
                    SecureEmptyStateIllustration(systemImage: "iphone.gen3", accent: SecurePalette.accentSky, size: 82)
                    SecureStatusBanner(
                        title: "No linked devices reported",
                        detail: "Device summaries appear here after the server reports linked identities.",
                        tone: .neutral,
                        systemImage: "ipad.landscape.badge.play"
                    )
                }
            } else {
                VStack(spacing: 10) {
                    ForEach(store.deviceSummaries, id: \.self) { line in
                        ClientDeviceModule(
                            summary: line,
                            current: !store.deviceDisplayID.isEmpty &&
                                line.localizedCaseInsensitiveContains(store.deviceDisplayID)
                        )
                    }
                }
            }
        }
        .secureCard()
    }
}

private struct SettingsStaticRow: View {
    let title: String
    let detail: String
    let systemImage: String
    let trailingValue: String

    private var accent: Color {
        switch systemImage {
        case "shield.fill", "lock.shield.fill":
            return SecurePalette.success
        case "iphone.gen3", "ipad.landscape":
            return SecurePalette.accentSky
        case "paintpalette.fill", "textformat":
            return SecurePalette.accentLavender
        case "bubble.left.and.bubble.right.fill":
            return SecurePalette.accent
        default:
            return SecurePalette.accent
        }
    }

    var body: some View {
        HStack(spacing: 12) {
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .fill(accent)
                .frame(width: 30, height: 30)
                .overlay(
                    Image(systemName: systemImage)
                        .font(.system(size: 13, weight: .semibold))
                        .foregroundStyle(Color.white)
                )

            VStack(alignment: .leading, spacing: 3) {
                Text(title)
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                if !detail.isEmpty {
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                        .lineLimit(1)
                }
            }

            Spacer(minLength: 12)

            if !trailingValue.isEmpty {
                Text(trailingValue)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(SecurePalette.textMuted)
            }
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 50)
        .contentShape(Rectangle())
    }
}

private struct SettingsActionRow: View {
    let title: String
    let detail: String
    let systemImage: String
    let isDestructive: Bool
    let action: () -> Void

    private var accent: Color {
        isDestructive ? SecurePalette.danger : SecurePalette.accent
    }

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                RoundedRectangle(cornerRadius: 9, style: .continuous)
                    .fill(accent)
                    .frame(width: 30, height: 30)
                    .overlay(
                        Image(systemName: systemImage)
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(Color.white)
                    )

                VStack(alignment: .leading, spacing: 3) {
                    Text(title)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(isDestructive ? SecurePalette.danger : SecurePalette.textPrimary)
                    if !detail.isEmpty {
                        Text(detail)
                            .font(.footnote)
                            .foregroundStyle(SecurePalette.textSecondary)
                            .lineLimit(1)
                    }
                }

                Spacer(minLength: 12)
            }
            .padding(.horizontal, 14)
            .frame(minHeight: 50)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

struct ClientWorkspaceView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        ClientConversationListCard(store: store)
        .navigationTitle("Chats")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
        .toolbar(.visible, for: .tabBar)
        .toolbar {
            if store.isLoggedIn {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: {}) {
                        Image(systemName: "square.and.pencil")
                    }
                    .accessibilityLabel("Compose chat")
                }
            }
        }
    }
}

struct ContactsHomeView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        SecureInsetGroupedListPage {
            Section {
                if store.contactConversations.isEmpty {
                    VStack(spacing: 10) {
                        SecureEmptyStateIllustration(systemImage: "person.2.fill", accent: SecurePalette.accentSky, size: 86)
                        SecureStatusBanner(
                            title: "No contacts available",
                            detail: "Contacts appear here after sign-in and incoming secure activity.",
                            tone: .neutral,
                            systemImage: "person.crop.circle.badge.questionmark"
                        )
                    }
                } else {
                    ForEach(store.contactConversations) { conversation in
                        SecureNavigationRow(
                            title: conversation.title,
                            detail: conversation.subtitle,
                            identityTitle: conversation.title,
                            identitySeed: conversation.id,
                            identityKind: .person,
                            identityPresence: .active,
                            destination: ClientConversationDetailView(
                                store: store,
                                conversation: conversation,
                                sourceTitle: "Contacts"
                            )
                        )
                    }
                }
            }
        }
        .navigationTitle("Contacts")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
    }
}

struct CallsHomeView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        SecureInsetGroupedListPage {
            Section {
                VStack(alignment: .leading, spacing: 12) {
                    SecureSectionHeader(
                        eyebrow: "Calls",
                        title: "Recent secure calls",
                        detail: "People and rooms lead the layout; call type and duration stay secondary."
                    )

                    HStack(spacing: 10) {
                        SecureIdentityAvatar(title: "Aster Stone", seed: "call-aster", kind: .person, size: 46, presence: .active)
                        SecureIdentityAvatar(title: "Threat Guild", seed: "call-threat", kind: .group, size: 46, presence: .secure)
                        SecureIdentityAvatar(title: "This device", seed: "call-device", kind: .device, size: 46, presence: .secure)
                    }
                }
                .secureCard(padding: 16)
                .listRowBackground(Color.clear)
                .listRowSeparator(.hidden)
            }

            Section {
                callRow(
                    title: "Aster Stone",
                    detail: "Video",
                    systemImage: "video.fill",
                    trailingValue: "01:26"
                )
                callRow(
                    title: "Threat Guild",
                    detail: "Room",
                    systemImage: "person.3.fill",
                    trailingValue: "01:24"
                )
                callRow(
                    title: "Ops Sync",
                    detail: "Voice",
                    systemImage: "phone.arrow.up.right",
                    trailingValue: "Yesterday"
                )
            }
        }
        .navigationTitle("Calls")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
    }

    private func callRow(title: String,
                         detail: String,
                         systemImage: String,
                         trailingValue: String) -> some View {
        HStack(spacing: 12) {
            SecureIdentityAvatar(
                title: title,
                seed: title,
                kind: detail == "Room" ? .group : .person,
                size: 40,
                presence: .secure
            )

            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(title)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    SecureMediaHintPill(kind: detail == "Video" ? .photo : (detail == "Voice" ? .voice : .file))
                }
                Text(detail)
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(1)
            }

            Spacer(minLength: 12)

            VStack(alignment: .trailing, spacing: 4) {
                Image(systemName: systemImage)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(SecurePalette.accent)
                Text(trailingValue)
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(SecurePalette.textMuted)
            }
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 56)
        .contentShape(Rectangle())
    }
}

struct TransportStatusView: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var gatewayDetail: String {
        store.serverHost.isEmpty ? "Not configured" : store.serverHost
    }

    private var deviceDetail: String {
        store.deviceDisplayID.isEmpty ? "Unavailable" : store.deviceDisplayID
    }

    var body: some View {
        SecureInsetGroupedListPage {
            if !store.remoteOK || !store.lastError.isEmpty {
                Section {
                    ClientInlineSecurityStatus(store: store)
                        .listRowBackground(Color.clear)
                }
            }

            Section {
                SettingsStaticRow(
                    title: "Session",
                    detail: "",
                    systemImage: "lock.shield.fill",
                    trailingValue: store.remoteOK ? "Secure" : "Review"
                )
                SettingsStaticRow(
                    title: "Gateway",
                    detail: gatewayDetail,
                    systemImage: "server.rack",
                    trailingValue: ""
                )
                SettingsStaticRow(
                    title: "This device",
                    detail: deviceDetail,
                    systemImage: "iphone.gen3",
                    trailingValue: ""
                )
            }

            if !store.isLoggedIn {
                Section {
                    SettingsStaticRow(
                        title: "Sign in",
                        detail: "",
                        systemImage: "rectangle.portrait.and.arrow.right",
                        trailingValue: ""
                    )
                }
            }
        }
        .navigationTitle("Transport Status")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
    }
}

struct SettingsHomeView: View {
    @ObservedObject var clientStore: ClientWorkspaceStore
    @ObservedObject var rootAuthStore: RootAuthStore

    var body: some View {
        SecureInsetGroupedListPage {
            Section {
                SecureNavigationRow(
                    title: "Security Center",
                    detail: "",
                    systemImage: "checkmark.shield.fill",
                    destination: SecurityCenterView(clientStore: clientStore, rootAuthStore: rootAuthStore)
                )
                SecureNavigationRow(
                    title: "Transport Status",
                    detail: "",
                    systemImage: "lock.shield.fill",
                    destination: TransportStatusView(store: clientStore)
                )
            }

            Section {
                SettingsStaticRow(
                    title: "Notifications",
                    detail: "",
                    systemImage: "bell.badge.fill",
                    trailingValue: ""
                )
                SettingsStaticRow(
                    title: "Appearance",
                    detail: "",
                    systemImage: "circle.lefthalf.filled",
                    trailingValue: ""
                )
            }

            Section {
                SettingsStaticRow(
                    title: "Signed in as",
                    detail: clientStore.username.isEmpty ? "Account" : clientStore.username,
                    systemImage: "person.crop.circle",
                    trailingValue: ""
                )
                SettingsActionRow(
                    title: "Sign out",
                    detail: "",
                    systemImage: "rectangle.portrait.and.arrow.right",
                    isDestructive: true,
                    action: { clientStore.signOut() }
                )
            }

            Section {
                ClientDevicesCard(store: clientStore)
                    .listRowInsets(EdgeInsets(top: 4, leading: 0, bottom: 4, trailing: 0))
                    .listRowSeparator(.hidden)
                    .listRowBackground(Color.clear)
            }
        }
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(.hidden, for: .navigationBar)
    }
}
