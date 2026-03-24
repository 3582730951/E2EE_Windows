import SwiftUI

private struct SecureNavigationRow<Destination: View>: View {
    let title: String
    let detail: String
    let systemImage: String
    let destination: Destination

    var body: some View {
        NavigationLink(destination: destination) {
            HStack(spacing: 14) {
                Image(systemName: systemImage)
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundStyle(SecurePalette.accent)
                    .frame(width: 32, height: 32)
                    .background(
                        Circle()
                            .fill(SecurePalette.surfaceRaised)
                    )

                VStack(alignment: .leading, spacing: 4) {
                    Text(title)
                        .font(.headline)
                        .foregroundStyle(SecurePalette.textPrimary)
                    Text(detail)
                        .font(.footnote)
                        .foregroundStyle(SecurePalette.textSecondary)
                }

                Spacer(minLength: 12)

                Image(systemName: "chevron.right")
                    .font(.footnote.weight(.semibold))
                    .foregroundStyle(SecurePalette.textMuted)
            }
            .padding(14)
            .background(
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .fill(SecurePalette.surfaceRaised)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 20, style: .continuous)
                    .stroke(SecurePalette.border, lineWidth: 1)
            )
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

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            SecureSectionHeader(
                eyebrow: "Access",
                title: "Sign in to the secure chat workspace",
                detail: "Configure transport first, then authenticate with account credentials and an optional root approval string."
            )

            VStack(alignment: .leading, spacing: 12) {
                Text("Transport")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textSecondary)
                TextField("Server host", text: $store.serverHost)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled(true)
                    .secureInput()
                TextField("Server port", text: $store.serverPort)
                    .keyboardType(.numberPad)
                    .secureInput()
                Toggle(isOn: $store.useTLS) {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("Use TLS")
                            .foregroundStyle(SecurePalette.textPrimary)
                        Text("Pin the remote fingerprint when available.")
                            .font(.caption)
                            .foregroundStyle(SecurePalette.textMuted)
                    }
                }
                .tint(SecurePalette.accent)
            }
            .secureCard(padding: 16)

            VStack(alignment: .leading, spacing: 12) {
                Text("Identity")
                    .font(.subheadline.weight(.semibold))
                    .foregroundStyle(SecurePalette.textSecondary)
                TextField("Username", text: $store.username)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled(true)
                    .secureInput()
                SecureField("Password", text: $store.password)
                    .secureInput()
                TextField("Root approval string (optional)", text: $store.rootCode)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled(true)
                    .secureInput()
            }
            .secureCard(padding: 16)

            HStack(spacing: 12) {
                Button(action: { store.registerAccount() }) {
                    Label("Create account", systemImage: "person.badge.plus")
                }
                .buttonStyle(SecureSecondaryButtonStyle())

                Button(action: { store.signIn() }) {
                    Label("Sign in", systemImage: "arrow.right.circle.fill")
                }
                .buttonStyle(SecurePrimaryButtonStyle())
            }
        }
        .secureCard()
    }
}

struct ClientConversationStrip: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Recents",
                title: "Active chats",
                detail: "Prioritize the current secure thread and keep switching friction low."
            )

            if store.conversations.isEmpty {
                SecureStatusBanner(
                    title: "No chats yet",
                    detail: "Create an account, add a contact, or wait for incoming encrypted events to populate the list.",
                    tone: .neutral,
                    systemImage: "bubble.left.and.text.bubble.right"
                )
            } else {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 12) {
                        ForEach(store.conversations) { conversation in
                            Button {
                                store.selectConversation(conversation.id)
                            } label: {
                                VStack(alignment: .leading, spacing: 10) {
                                    HStack {
                                        Image(systemName: conversation.isGroup ? "person.3.fill" : "message.fill")
                                            .font(.system(size: 13, weight: .semibold))
                                            .foregroundStyle(store.selectedConversationID == conversation.id ? Color.white : SecurePalette.accent)
                                        Spacer(minLength: 8)
                                        Text(conversation.isGroup ? "GROUP" : "DM")
                                            .font(.caption2.weight(.semibold))
                                            .tracking(1.0)
                                            .foregroundStyle(store.selectedConversationID == conversation.id ? Color.white.opacity(0.82) : SecurePalette.textMuted)
                                    }

                                    Text(conversation.title)
                                        .font(.headline)
                                        .foregroundStyle(store.selectedConversationID == conversation.id ? Color.white : SecurePalette.textPrimary)
                                        .lineLimit(1)

                                    Text(conversation.subtitle)
                                        .font(.footnote)
                                        .foregroundStyle(store.selectedConversationID == conversation.id ? Color.white.opacity(0.78) : SecurePalette.textSecondary)
                                        .lineLimit(2)
                                }
                                .frame(width: 220, alignment: .leading)
                                .padding(16)
                                .background(
                                    RoundedRectangle(cornerRadius: 24, style: .continuous)
                                        .fill(
                                            store.selectedConversationID == conversation.id
                                                ? SecurePalette.accent
                                                : SecurePalette.surfaceRaised
                                        )
                                )
                                .overlay(
                                    RoundedRectangle(cornerRadius: 24, style: .continuous)
                                        .stroke(
                                            store.selectedConversationID == conversation.id
                                                ? Color.white.opacity(0.18)
                                                : SecurePalette.border,
                                            lineWidth: 1
                                        )
                                )
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .padding(.vertical, 2)
                }
            }
        }
        .secureCard()
    }
}

struct ClientMessagesCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    private let formatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm"
        return formatter
    }()

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack {
                VStack(alignment: .leading, spacing: 6) {
                    Text("Conversation")
                        .font(.caption.weight(.semibold))
                        .tracking(1.1)
                        .foregroundStyle(SecurePalette.textMuted)
                    Text(store.currentConversationTitle)
                        .font(.title3.weight(.semibold))
                        .foregroundStyle(SecurePalette.textPrimary)
                }
                Spacer(minLength: 12)
                Text("\(store.currentMessages.count) msgs")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(SecurePalette.textSecondary)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 8)
                    .background(
                        Capsule()
                            .fill(SecurePalette.surfaceRaised)
                    )
            }

            if store.currentMessages.isEmpty {
                SecureStatusBanner(
                    title: "No encrypted messages yet",
                    detail: "Once a thread is selected, inbound and outbound messages will appear here with clearer sender and time hierarchy.",
                    tone: .neutral,
                    systemImage: "text.bubble"
                )
            } else {
                ScrollView {
                    LazyVStack(spacing: 10) {
                        ForEach(store.currentMessages) { message in
                            HStack {
                                if message.outgoing {
                                    Spacer(minLength: 56)
                                }

                                VStack(alignment: .leading, spacing: 6) {
                                    HStack(spacing: 8) {
                                        Text(message.outgoing ? "You" : message.sender)
                                            .font(.caption.weight(.semibold))
                                            .foregroundStyle(message.outgoing ? Color.white.opacity(0.86) : SecurePalette.textSecondary)
                                        Text(formatter.string(from: Date(timeIntervalSince1970: TimeInterval(message.timestampMS) / 1000.0)))
                                            .font(.caption2)
                                            .foregroundStyle(message.outgoing ? Color.white.opacity(0.72) : SecurePalette.textMuted)
                                    }

                                    Text(message.text)
                                        .font(.body)
                                        .foregroundStyle(message.outgoing ? Color.white : SecurePalette.textPrimary)
                                        .frame(maxWidth: .infinity, alignment: .leading)
                                }
                                .padding(.horizontal, 14)
                                .padding(.vertical, 12)
                                .frame(maxWidth: 320, alignment: .leading)
                                .background(
                                    RoundedRectangle(cornerRadius: 22, style: .continuous)
                                        .fill(message.outgoing ? SecurePalette.accent : SecurePalette.surfaceRaised)
                                )
                                .overlay(
                                    RoundedRectangle(cornerRadius: 22, style: .continuous)
                                        .stroke(message.outgoing ? Color.white.opacity(0.12) : SecurePalette.border, lineWidth: 1)
                                )

                                if !message.outgoing {
                                    Spacer(minLength: 56)
                                }
                            }
                        }
                    }
                    .frame(maxWidth: .infinity)
                }
                .frame(minHeight: 240, maxHeight: 360)
            }
        }
        .secureCard()
    }
}

struct ClientComposerCard: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            SecureSectionHeader(
                eyebrow: "Composer",
                title: "Send the next encrypted message",
                detail: store.selectedConversationID.isEmpty
                    ? "Select a chat first."
                    : "Draft stays local until you send it."
            )

            TextEditor(text: $store.draft)
                .frame(minHeight: 88)
                .padding(8)
                .background(
                    RoundedRectangle(cornerRadius: 20, style: .continuous)
                        .fill(SecurePalette.surfaceRaised)
                )
                .overlay(
                    RoundedRectangle(cornerRadius: 20, style: .continuous)
                        .stroke(SecurePalette.borderStrong, lineWidth: 1)
                )
                .foregroundStyle(SecurePalette.textPrimary)

            Button(action: { store.sendDraft() }) {
                Label("Send secure message", systemImage: "paperplane.fill")
            }
            .buttonStyle(SecurePrimaryButtonStyle())
            .disabled(store.selectedConversationID.isEmpty || store.draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
        }
        .secureCard()
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

struct ClientWorkspaceView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(spacing: 18) {
                    ClientStatusCard(store: store)
                    if store.isLoggedIn {
                        ClientConversationStrip(store: store)
                        ClientMessagesCard(store: store)
                        ClientComposerCard(store: store)
                        ClientDevicesCard(store: store)
                    } else {
                        ClientLoginCard(store: store)
                    }
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 20)
            }
        }
        .navigationTitle("Chats")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct ContactsHomeView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(spacing: 18) {
                    SecureSectionHeader(
                        eyebrow: "People",
                        title: "Contacts and direct threads",
                        detail: "Keep the contact roster close to the active chat shell instead of burying it behind tools."
                    )
                    .secureCard()

                    if store.contactConversations.isEmpty {
                        SecureStatusBanner(
                            title: "No contacts available",
                            detail: "Contacts appear here after sign-in and incoming secure activity.",
                            tone: .neutral,
                            systemImage: "person.crop.circle.badge.questionmark"
                        )
                        .secureCard()
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
                        .secureCard()
                    }
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 20)
            }
        }
        .navigationTitle("Contacts")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct CallsHomeView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(spacing: 18) {
                    SecureSectionHeader(
                        eyebrow: "Calls",
                        title: "Call hub",
                        detail: "Call state, active rooms, and media-related diagnostics stay visible without leaving the main product shell."
                    )
                    .secureCard()

                    SecureStatusBanner(
                        title: store.isLoggedIn ? "Ready for secure calls" : "Sign in required",
                        detail: store.isLoggedIn
                            ? "Peer and group call flows are coordinated from the main client workspace."
                            : "Call controls become available after the secure session is established.",
                        tone: store.isLoggedIn ? .success : .warning,
                        systemImage: store.isLoggedIn ? "phone.connection.fill" : "phone.down.waves.left.and.right"
                    )
                    .secureCard()

                    ClientDevicesCard(store: store)
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 20)
            }
        }
        .navigationTitle("Calls")
        .navigationBarTitleDisplayMode(.inline)
    }
}

struct TransportStatusView: View {
    @ObservedObject var store: ClientWorkspaceStore

    var body: some View {
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(spacing: 18) {
                    SecureSectionHeader(
                        eyebrow: "Transport",
                        title: "Connection, session, and device status",
                        detail: "Keep session health visible from Settings without pushing the full chat workspace into the settings stack."
                    )
                    .secureCard()

                    ClientStatusCard(store: store)

                    if !store.isLoggedIn {
                        ClientLoginCard(store: store)
                    }
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 20)
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
        ZStack {
            SecureSceneBackground()

            ScrollView(showsIndicators: false) {
                VStack(spacing: 18) {
                    SecureSectionHeader(
                        eyebrow: "Settings",
                        title: "System, privacy, and trust",
                        detail: "Move identity, device, and root authorization under one predictable settings hierarchy."
                    )
                    .secureCard()

                    VStack(spacing: 12) {
                        SecureNavigationRow(
                            title: "Security Center",
                            detail: "Device trust, root authorization, and linked device state.",
                            systemImage: "checkmark.shield.fill",
                            destination: SecurityCenterView(clientStore: clientStore, rootAuthStore: rootAuthStore)
                        )

                        SecureNavigationRow(
                            title: "Transport Status",
                            detail: clientStore.remoteOK ? "Encrypted session healthy." : "Session needs attention.",
                            systemImage: "lock.shield.fill",
                            destination: TransportStatusView(store: clientStore)
                        )
                    }
                    .secureCard()
                }
                .padding(.horizontal, 18)
                .padding(.vertical, 20)
            }
        }
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(.inline)
    }
}
