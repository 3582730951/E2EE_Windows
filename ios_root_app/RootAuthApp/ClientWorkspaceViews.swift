import Foundation
import SwiftUI

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

private struct SecureNavigationRow<Destination: View>: View {
    let title: String
    let detail: String
    let systemImage: String
    var badge: String? = nil
    let destination: Destination

    var body: some View {
        NavigationLink(destination: destination) {
            HStack(spacing: 10) {
                Image(systemName: systemImage)
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(SecurePalette.accent)
                    .frame(width: 22, height: 22)

                VStack(alignment: .leading, spacing: 4) {
                    HStack(spacing: 8) {
                        Text(title)
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(SecurePalette.textPrimary)
                        if let badge, !badge.isEmpty {
                            Text(badge.uppercased())
                                .font(.caption2.weight(.bold))
                                .tracking(0.8)
                                .foregroundStyle(SecurePalette.accent)
                                .padding(.horizontal, 7)
                                .padding(.vertical, 3)
                                .background(
                                    Capsule()
                                        .fill(SecurePalette.accentSoft)
                                )
                        }
                    }
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                        .lineLimit(1)
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

struct ClientStatusCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var tone: SecureBannerTone {
        if !store.lastError.isEmpty && !store.remoteOK {
            return .danger
        }
        return store.remoteOK ? .success : .neutral
    }

    private var statusDetail: String {
        if !store.lastError.isEmpty {
            return store.lastError
        }
        return store.statusText
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("SECURE CLIENT")
                        .font(.caption.weight(.semibold))
                        .tracking(1.2)
                        .foregroundStyle(SecurePalette.textMuted)
                    Text(store.isLoggedIn ? "Conversation workspace" : "Prepare this device")
                        .font(.title3.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text("A denser, calmer chat shell with security context built into the primary product flow.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }

                Spacer(minLength: 12)

                HStack(spacing: 8) {
                    Circle()
                        .fill(store.remoteOK ? SecurePalette.success : SecurePalette.warning)
                        .frame(width: 8, height: 8)
                    Text(store.remoteOK ? "Secure" : "Checking")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(
                    Capsule()
                        .fill(SecurePalette.surfaceRaised)
                )
            }

            SecureStatusBanner(
                title: store.remoteOK ? "Encrypted session ready" : "Session status",
                detail: statusDetail,
                tone: tone,
                systemImage: store.remoteOK ? "lock.shield.fill" : "wave.3.right.circle"
            )

            HStack(spacing: 12) {
                SecureMetricTile(
                    label: "Server",
                    value: "\(store.serverHost):\(store.serverPort)",
                    icon: "server.rack"
                )
                SecureMetricTile(
                    label: "Device",
                    value: store.deviceDisplayID.isEmpty ? "Pending" : store.deviceDisplayID,
                    icon: "iphone.gen3"
                )
            }

            HStack(spacing: 12) {
                Button(action: { store.refreshNow() }) {
                    Label("Refresh", systemImage: "arrow.clockwise")
                }
                .buttonStyle(SecureSecondaryButtonStyle())

                if store.isLoggedIn {
                    Button(action: { store.signOut() }) {
                        Label("Sign out", systemImage: "rectangle.portrait.and.arrow.right")
                    }
                    .buttonStyle(SecureSecondaryButtonStyle())
                }
            }
        }
        .secureCard(padding: 20)
    }
}

struct ClientLoginCard: View {
    @ObservedObject var store: ClientWorkspaceStore
    @State private var showsAdvancedApproval = false

    private var transportSummary: String {
        "\(store.serverHost):\(store.serverPort) • \(store.useTLS ? "TLS" : "TCP")"
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SecureSectionHeader(
                eyebrow: "Access",
                title: "Sign in to your secure chats",
                detail: "Use your account credentials first."
            )

            ClientSecuritySummaryCard(store: store)

            VStack(alignment: .leading, spacing: 6) {
                TextField("Username", text: $store.username)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled(true)
                    .secureInput()
                SecureField("Password", text: $store.password)
                    .secureInput()
            }

            Button(action: { store.signIn() }) {
                Label("Sign in", systemImage: "arrow.right.circle.fill")
            }
            .buttonStyle(SecurePrimaryButtonStyle())

            HStack {
                Button(action: { store.registerAccount() }) {
                    Text("Create account")
                        .font(.footnote.weight(.semibold))
                        .foregroundStyle(SecurePalette.accent)
                        .padding(.vertical, 2)
                }
                .buttonStyle(.plain)

                Spacer(minLength: 12)

                Button(action: {
                    withAnimation(.easeInOut(duration: 0.18)) {
                        showsAdvancedApproval.toggle()
                    }
                }) {
                    Text("Advanced sign-in")
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(SecurePalette.textMuted)
                        .padding(.vertical, 2)
                }
                .buttonStyle(.plain)
            }

            if showsAdvancedApproval {
                VStack(alignment: .leading, spacing: 8) {
                    Text(transportSummary)
                        .font(.caption)
                        .foregroundStyle(SecurePalette.textSecondary)
                        .lineLimit(1)

                    TextField("Server host", text: $store.serverHost)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled(true)
                        .secureInput()

                    HStack(spacing: 10) {
                        TextField("Server port", text: $store.serverPort)
                            .keyboardType(.numberPad)
                            .secureInput()

                        Toggle(isOn: $store.useTLS) {
                            Text("TLS")
                                .font(.footnote.weight(.semibold))
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

                    TextField("Approval string", text: $store.rootCode)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled(true)
                        .secureInput()
                }
                .padding(12)
                .background(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .fill(SecurePalette.surfaceRaised.opacity(0.72))
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .stroke(SecurePalette.border, lineWidth: 1)
                )
            }
        }
        .secureCard(padding: 14)
    }
}

struct ClientAuthShellView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 14,
                                   verticalPadding: 10,
                                   showsIndicators: false) {
            VStack(spacing: 8) {
                ClientAuthHeader()
                ClientLoginCard(store: store)
            }
            .frame(maxWidth: 420, alignment: .top)
            .frame(maxWidth: .infinity, alignment: .top)
        }
        .navigationTitle("Secure Chat")
        .navigationBarTitleDisplayMode(.inline)
    }
}

private struct ClientAuthHeader: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                ZStack {
                    RoundedRectangle(cornerRadius: 12, style: .continuous)
                        .fill(SecurePalette.accentSoft)
                        .frame(width: 36, height: 36)
                    Image(systemName: "bubble.left.and.bubble.right.fill")
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(SecurePalette.accent)
                }

                VStack(alignment: .leading, spacing: 3) {
                    Text("Secure Chat")
                        .font(.title2.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text("Sign in to get back to messages, calls, and linked devices.")
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

private struct ClientSecuritySummaryCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var statusText: String {
        if !store.lastError.isEmpty {
            return store.lastError
        }
        return store.remoteOK ? "End-to-end secure session active." : "Session validation in progress."
    }

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: store.remoteOK ? "lock.shield.fill" : "wave.3.right.circle.fill")
                .font(.system(size: 12, weight: .semibold))
                .foregroundStyle(store.remoteOK ? SecurePalette.success : SecurePalette.warning)
                .frame(width: 18, height: 18)
                .background(
                    Circle()
                        .fill(SecurePalette.surfaceRaised)
                )

            VStack(alignment: .leading, spacing: 3) {
                Text(store.remoteOK ? "Secure transport healthy" : "Secure transport checking")
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                Text(statusText)
                    .font(.caption2)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(1)
            }

            Spacer(minLength: 8)

            SecureCircularIconButton(
                systemImage: "arrow.clockwise",
                accessibilityLabel: "Refresh secure session status",
                iconSize: 10,
                buttonSize: 28,
                foreground: SecurePalette.accent
            ) {
                store.refreshNow()
            }
        }
        .padding(.horizontal, 10)
        .frame(height: 36)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(SecurePalette.surface.opacity(0.92))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
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

    private var detail: String {
        if !store.lastError.isEmpty {
            return store.lastError
        }
        return store.remoteOK ? "Encrypted session active" : "Verifying secure transport"
    }

    var body: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(tint)
                .frame(width: 6, height: 6)

            Text(store.remoteOK ? "Secure" : "Checking")
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textPrimary)

            Text(detail)
                .font(.caption)
                .foregroundStyle(SecurePalette.textSecondary)
                .lineLimit(1)

            Spacer(minLength: 8)

            SecureCircularIconButton(
                systemImage: "arrow.clockwise",
                accessibilityLabel: "Refresh session state",
                iconSize: 11,
                buttonSize: 26,
                foreground: SecurePalette.accent
            ) {
                store.refreshNow()
            }
        }
        .padding(.horizontal, 10)
        .frame(height: 28)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(SecurePalette.surface.opacity(0.90))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .stroke(SecurePalette.border, lineWidth: 1)
        )
    }
}

private struct ClientConversationRow: View {
    let conversation: ClientConversation
    let preview: String
    let timestampMS: UInt64
    let isSelected: Bool

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

    private var avatarText: String {
        let trimmed = conversation.title.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let first = trimmed.first else {
            return "#"
        }
        return String(first).uppercased()
    }

    var body: some View {
        HStack(spacing: 10) {
            ZStack(alignment: .bottomTrailing) {
                Circle()
                    .fill(
                        LinearGradient(colors: [
                            SecurePalette.accent.opacity(0.88),
                            SecurePalette.accent.opacity(0.58)
                        ], startPoint: .topLeading, endPoint: .bottomTrailing)
                    )
                    .frame(width: 44, height: 44)
                Text(avatarText)
                    .font(.footnote.weight(.bold))
                    .foregroundStyle(Color.white)

                if conversation.isGroup {
                    Image(systemName: "person.3.fill")
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(Color.white.opacity(0.9))
                        .padding(4)
                        .background(
                            Circle()
                                .fill(SecurePalette.surface)
                        )
                        .offset(x: 2, y: 2)
                }
            }

            VStack(alignment: .leading, spacing: 3) {
                HStack(spacing: 8) {
                    Text(conversation.title)
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                        .lineLimit(1)

                    Spacer(minLength: 8)

                    if !timestampLabel.isEmpty {
                        Text(timestampLabel)
                            .font(.system(size: 12, weight: .medium))
                            .foregroundStyle(SecurePalette.textMuted)
                    }
                }

                Text(preview)
                    .font(.system(size: 13))
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(1)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.horizontal, 6)
        .frame(height: 68)
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

    private var orderedConversations: [ClientConversation] {
        store.conversations.sorted { lhs, rhs in
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

    var body: some View {
        let rows = filteredConversations

        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 8) {
                Image(systemName: "magnifyingglass")
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundStyle(SecurePalette.textMuted)
                TextField("Search conversations", text: $query)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled(true)
                    .font(.system(size: 15))
            }
            .padding(.horizontal, 12)
            .frame(height: 36)
            .background(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .stroke(SecurePalette.borderStrong, lineWidth: 1)
            )

            ClientInlineSecurityStatus(store: store)

            if rows.isEmpty {
                SecureStatusBanner(
                    title: store.conversations.isEmpty ? "No chats yet" : "No matching chats",
                    detail: store.conversations.isEmpty
                        ? "Start a secure conversation once contacts or incoming events are available."
                        : "Try a different keyword.",
                    tone: .neutral,
                    systemImage: "bubble.left.and.text.bubble.right"
                )
            } else {
                LazyVStack(spacing: 0) {
                    ForEach(Array(rows.enumerated()), id: \.element.id) { index, conversation in
                        let preview = store.latestMessage(for: conversation.id)?.text ?? conversation.subtitle
                        let timestamp = store.latestMessage(for: conversation.id)?.timestampMS ?? 0

                        NavigationLink {
                            ClientConversationDetailView(store: store, conversation: conversation)
                                .onAppear {
                                    store.selectConversation(conversation.id)
                                }
                        } label: {
                            ClientConversationRow(
                                conversation: conversation,
                                preview: preview,
                                timestampMS: timestamp,
                                isSelected: store.selectedConversationID == conversation.id
                            )
                        }
                        .buttonStyle(.plain)

                        if index < rows.count - 1 {
                            Divider()
                                .overlay(SecurePalette.border)
                                .padding(.leading, 66)
                        }
                    }
                }
            }
        }
        .padding(.top, 0)
    }
}

private struct ClientConversationTitleView: View {
    let conversation: ClientConversation
    let status: String

    var body: some View {
        Text(conversation.title)
            .font(.subheadline.weight(.semibold))
            .foregroundStyle(SecurePalette.textPrimary)
            .lineLimit(1)
            .frame(maxWidth: 184, alignment: .center)
            .accessibilityLabel("\(conversation.title), \(status)")
    }
}

struct ClientConversationDetailView: View {
    @ObservedObject var store: ClientWorkspaceStore
    let conversation: ClientConversation
    @Environment(\.dismiss) private var dismiss

    private var conversationStatus: String {
        conversation.isGroup ? "Secure group" : "Last seen"
    }

    var body: some View {
        ZStack {
            SecureSceneBackground()
                .ignoresSafeArea()

            ClientMessagesCard(store: store, showsThreadHeader: false)
                .padding(.horizontal, 10)
                .padding(.top, 2)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        }
        .safeAreaInset(edge: .bottom) {
            ClientComposerCard(store: store, compact: true)
                .padding(.horizontal, 8)
                .background(
                    SecurePalette.composerSurface.opacity(0.98)
                        .ignoresSafeArea(edges: .bottom)
                )
        }
        .navigationTitle("")
        .navigationBarTitleDisplayMode(.inline)
        .navigationBarBackButtonHidden(true)
        .toolbar(.hidden, for: .tabBar)
        .toolbar {
            ToolbarItem(placement: .navigationBarLeading) {
                Button(action: { dismiss() }) {
                    HStack(spacing: 3) {
                        Image(systemName: "chevron.left")
                            .font(.footnote.weight(.semibold))
                        Text("Chats")
                            .font(.footnote)
                    }
                }
                .accessibilityLabel("Back to chats")
            }

            ToolbarItem(placement: .principal) {
                ClientConversationTitleView(
                    conversation: conversation,
                    status: conversationStatus
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

                Button(action: { store.refreshNow() }) {
                    Image(systemName: "ellipsis.circle")
                        .font(.system(size: 16, weight: .semibold))
                }
                .accessibilityLabel("More conversation actions")
            }
        }
        .onAppear {
            store.selectConversation(conversation.id)
            store.refreshNow()
        }
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
                    LazyVStack(spacing: 4) {
                        ForEach(store.currentMessages) { message in
                            HStack {
                                if message.outgoing {
                                    Spacer(minLength: 48)
                                }

                                VStack(alignment: .leading, spacing: 3) {
                                    HStack(spacing: 8) {
                                        Text(message.outgoing ? "You" : message.sender)
                                            .font(.caption2.weight(.semibold))
                                            .foregroundStyle(message.outgoing ? Color.white.opacity(0.86) : SecurePalette.textSecondary)
                                        Text(formatter.string(from: Date(timeIntervalSince1970: TimeInterval(message.timestampMS) / 1000.0)))
                                            .font(.caption2)
                                            .foregroundStyle(message.outgoing ? Color.white.opacity(0.72) : SecurePalette.textMuted)
                                    }

                                    Text(message.text)
                                        .font(.system(size: 15))
                                        .foregroundStyle(message.outgoing ? Color.white : SecurePalette.textPrimary)
                                        .frame(maxWidth: .infinity, alignment: .leading)
                                }
                                .padding(.horizontal, 10)
                                .padding(.vertical, 6)
                                .frame(maxWidth: 252, alignment: .leading)
                                .background(
                                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                                        .fill(message.outgoing ? SecurePalette.outgoingBubble : SecurePalette.surfaceRaised)
                                )
                                .overlay(
                                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                                        .stroke(message.outgoing ? Color.white.opacity(0.12) : SecurePalette.border, lineWidth: 1)
                                )

                                if !message.outgoing {
                                    Spacer(minLength: 48)
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
        HStack(alignment: .center, spacing: compact ? 6 : 10) {
            Button(action: {}) {
                Image(systemName: "plus")
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(SecurePalette.textPrimary)
                    .frame(width: compact ? 32 : 36, height: compact ? 32 : 36)
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

            TextField("Message", text: $store.draft, axis: .vertical)
                .lineLimit(1...1)
                .frame(minHeight: compact ? 32 : 38)
                .padding(.horizontal, compact ? 10 : 12)
                .padding(.vertical, compact ? 0 : 8)
                .background(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .fill(SecurePalette.composerSurface)
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 14, style: .continuous)
                        .stroke(SecurePalette.borderStrong, lineWidth: 1)
                )
                .foregroundStyle(SecurePalette.textPrimary)

            Button(action: { store.sendDraft() }) {
                Image(systemName: "paperplane.fill")
                    .font(.system(size: compact ? 14 : 16, weight: .semibold))
                    .foregroundStyle(Color.white)
                    .frame(width: compact ? 32 : 38, height: compact ? 32 : 38)
                    .background(
                        Circle()
                            .fill(canSend ? SecurePalette.outgoingBubble : SecurePalette.outgoingBubble.opacity(0.45))
                    )
            }
            .buttonStyle(.plain)
            .disabled(!canSend)
        }
        .frame(height: compact ? 44 : 52)
        .padding(.horizontal, compact ? 0 : 7)
    }
}

struct ClientDevicesCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Devices",
                title: "Linked device inventory",
                detail: "Track which device identifiers are active for this account."
            )

            if store.deviceSummaries.isEmpty {
                SecureStatusBanner(
                    title: "No linked devices reported",
                    detail: "Device summaries appear here after the server reports linked identities.",
                    tone: .neutral,
                    systemImage: "ipad.landscape.badge.play"
                )
            } else {
                VStack(spacing: 10) {
                    ForEach(store.deviceSummaries, id: \.self) { line in
                        HStack(spacing: 12) {
                            Image(systemName: "checkmark.shield")
                                .foregroundStyle(SecurePalette.success)
                            Text(line)
                                .font(.footnote)
                                .foregroundStyle(SecurePalette.textPrimary)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
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
            }
        }
        .secureCard()
    }
}

private struct SettingsGroupLabel: View {
    let title: String

    var body: some View {
        Text(title.uppercased())
            .font(.caption.weight(.semibold))
            .tracking(1.0)
            .foregroundStyle(SecurePalette.textMuted)
            .padding(.horizontal, 4)
    }
}

private struct SettingsGroupContainer<Content: View>: View {
    let content: () -> Content

    init(@ViewBuilder content: @escaping () -> Content) {
        self.content = content
    }

    var body: some View {
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
}

private struct SettingsRowDivider: View {
    var body: some View {
        Divider()
            .overlay(SecurePalette.border)
            .padding(.leading, 54)
    }
}

private struct SettingsStaticRow: View {
    let title: String
    let detail: String
    let systemImage: String
    let trailingValue: String

    var body: some View {
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
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(1)
            }

            Spacer(minLength: 12)

            Text(trailingValue)
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textMuted)
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 54)
        .contentShape(Rectangle())
    }
}

private struct SettingsActionRow: View {
    let title: String
    let detail: String
    let systemImage: String
    let isDestructive: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: systemImage)
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(isDestructive ? SecurePalette.danger : SecurePalette.accent)
                    .frame(width: 22, height: 22)

                VStack(alignment: .leading, spacing: 3) {
                    Text(title)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(isDestructive ? SecurePalette.danger : SecurePalette.textPrimary)
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                        .lineLimit(1)
                }

                Spacer(minLength: 12)
            }
            .padding(.horizontal, 14)
            .frame(minHeight: 54)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

struct ClientWorkspaceView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 12,
                                   verticalPadding: 4,
                                   showsIndicators: false) {
            ClientConversationListCard(store: store)
        }
        .navigationTitle("Chats")
        .navigationBarTitleDisplayMode(.inline)
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
        SecureFullscreenScrollPage(horizontalPadding: 14,
                                   verticalPadding: 16,
                                   showsIndicators: false) {
            VStack(spacing: 14) {
                SecureSectionHeader(
                    eyebrow: "People",
                    title: "Contacts and direct threads",
                    detail: "Keep the contact roster close to the active chat shell instead of burying it behind tools."
                )

                if store.contactConversations.isEmpty {
                    SecureStatusBanner(
                        title: "No contacts available",
                        detail: "Contacts appear here after sign-in and incoming secure activity.",
                        tone: .neutral,
                        systemImage: "person.crop.circle.badge.questionmark"
                    )
                } else {
                    VStack(spacing: 10) {
                        ForEach(store.contactConversations) { conversation in
                            VStack(alignment: .leading, spacing: 6) {
                                Text(conversation.title)
                                    .font(.headline)
                                    .foregroundStyle(SecurePalette.textPrimary)
                                Text(conversation.subtitle)
                                    .font(.footnote)
                                    .foregroundStyle(SecurePalette.textSecondary)
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(16)
                            .background(
                                RoundedRectangle(cornerRadius: 20, style: .continuous)
                                    .fill(SecurePalette.surfaceRaised)
                            )
                            .overlay(
                                RoundedRectangle(cornerRadius: 20, style: .continuous)
                                    .stroke(SecurePalette.border, lineWidth: 1)
                            )
                        }
                    }
                }
            }
        }
        .navigationTitle("Contacts")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct CallsHomeView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 12,
                                   verticalPadding: 8,
                                   showsIndicators: false) {
            VStack(alignment: .leading, spacing: 12) {
                SettingsGroupContainer {
                    callRow(
                        title: "Aster Stone",
                        detail: "Recent video call",
                        systemImage: "phone.fill",
                        trailingValue: "01:26"
                    )
                    SettingsRowDivider()
                    callRow(
                        title: "Threat Guild",
                        detail: "Recent room call",
                        systemImage: "person.3.fill",
                        trailingValue: "01:24"
                    )
                    SettingsRowDivider()
                    callRow(
                        title: "Ops Sync",
                        detail: "Recent voice call",
                        systemImage: "phone.arrow.up.right",
                        trailingValue: "Yesterday"
                    )
                }
            }
        }
        .navigationTitle("Calls")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func callRow(title: String,
                         detail: String,
                         systemImage: String,
                         trailingValue: String) -> some View {
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
                    .font(.footnote)
                    .foregroundStyle(SecurePalette.textSecondary)
                    .lineLimit(1)
            }

            Spacer(minLength: 12)

            Text(trailingValue)
                .font(.caption.weight(.semibold))
                .foregroundStyle(SecurePalette.textMuted)
        }
        .padding(.horizontal, 14)
        .frame(minHeight: 64)
        .contentShape(Rectangle())
    }
}

struct TransportStatusView: View {
    @ObservedObject var store: ClientWorkspaceStore

    private var transportDetail: String {
        if !store.lastError.isEmpty {
            return "Session needs attention."
        }
        return store.remoteOK ? "Encrypted session active." : "Waiting for secure transport."
    }

    private var gatewayDetail: String {
        store.serverHost.isEmpty ? "Not configured" : store.serverHost
    }

    private var deviceDetail: String {
        store.deviceDisplayID.isEmpty ? "Unavailable" : store.deviceDisplayID
    }

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 12,
                                   verticalPadding: 10,
                                   showsIndicators: false) {
            VStack(alignment: .leading, spacing: 8) {
                ClientInlineSecurityStatus(store: store)

                SettingsGroupLabel(title: "Transport")
                SettingsGroupContainer {
                    SettingsStaticRow(
                        title: "Session",
                        detail: transportDetail,
                        systemImage: "lock.shield.fill",
                        trailingValue: store.remoteOK ? "Healthy" : "Review"
                    )
                    SettingsRowDivider()
                    SettingsStaticRow(
                        title: "Gateway",
                        detail: gatewayDetail,
                        systemImage: "server.rack",
                        trailingValue: "\(store.serverPort) \(store.useTLS ? "TLS" : "TCP")"
                    )
                    SettingsRowDivider()
                    SettingsStaticRow(
                        title: "This device",
                        detail: deviceDetail,
                        systemImage: "iphone.gen3",
                        trailingValue: store.isLoggedIn ? "Active" : "Sign in"
                    )
                }

                if !store.isLoggedIn {
                    SettingsGroupLabel(title: "Access")
                    SettingsGroupContainer {
                        SettingsStaticRow(
                            title: "Sign in required",
                            detail: "Return to sign in to reconnect this device.",
                            systemImage: "rectangle.portrait.and.arrow.right",
                            trailingValue: "Required"
                        )
                    }
                }
            }
        }
        .navigationTitle("Transport Status")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct SettingsHomeView: View {
    @ObservedObject var clientStore: ClientWorkspaceStore
    @ObservedObject var rootAuthStore: RootAuthStore

    var body: some View {
        SecureFullscreenScrollPage(horizontalPadding: 12,
                                   verticalPadding: 10,
                                   showsIndicators: false) {
            VStack(alignment: .leading, spacing: 8) {
                SettingsGroupLabel(title: "Trust & Access")

                SettingsGroupContainer {
                    SecureNavigationRow(
                        title: "Security Center",
                        detail: clientStore.remoteOK
                            ? "Devices, trust, and approval details."
                            : "Review device trust and session state.",
                        systemImage: "checkmark.shield.fill",
                        destination: SecurityCenterView(clientStore: clientStore, rootAuthStore: rootAuthStore)
                    )
                    SettingsRowDivider()
                    SecureNavigationRow(
                        title: "Transport Status",
                        detail: clientStore.remoteOK ? "Encrypted session healthy." : "Session needs attention.",
                        systemImage: "lock.shield.fill",
                        destination: TransportStatusView(store: clientStore)
                    )
                }

                SettingsGroupLabel(title: "Preferences")

                SettingsGroupContainer {
                    SettingsStaticRow(
                        title: "Notifications",
                        detail: "Mentions, message alerts, and call prompts",
                        systemImage: "bell.badge.fill",
                        trailingValue: "On"
                    )
                    SettingsRowDivider()
                    SettingsStaticRow(
                        title: "Appearance",
                        detail: "Light, dark, and system display mode",
                        systemImage: "circle.lefthalf.filled",
                        trailingValue: "System"
                    )
                }

                SettingsGroupLabel(title: "Account")

                SettingsGroupContainer {
                    SettingsStaticRow(
                        title: "Signed in as",
                        detail: clientStore.username.isEmpty ? "Secure account" : clientStore.username,
                        systemImage: "person.crop.circle",
                        trailingValue: clientStore.deviceDisplayID.isEmpty ? "Device" : clientStore.deviceDisplayID
                    )
                    SettingsRowDivider()
                    SettingsActionRow(
                        title: "Sign out",
                        detail: "Disconnect this device from the current secure session.",
                        systemImage: "rectangle.portrait.and.arrow.right",
                        isDestructive: true,
                        action: { clientStore.signOut() }
                    )
                }
            }
        }
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(.inline)
    }
}
