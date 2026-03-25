import Foundation
import SwiftUI
import UIKit

private enum ScreenshotScenario: String {
    case none
    case chats
    case detail
    case security

    static var current: ScreenshotScenario {
        let raw = ProcessInfo.processInfo.environment["MI_E2EE_IOS_SCREENSHOT_MODE"]?
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased() ?? ""
        return ScreenshotScenario(rawValue: raw) ?? .none
    }
}

struct ClientConversation: Identifiable, Hashable {
    let id: String
    let title: String
    let subtitle: String
    let isGroup: Bool
}

struct ClientMessage: Identifiable, Hashable {
    let id: String
    let conversationID: String
    let sender: String
    let text: String
    let outgoing: Bool
    let timestampMS: UInt64
}

private enum ClientBootstrapError: LocalizedError {
    case appSupportUnavailable

    var errorDescription: String? {
        switch self {
        case .appSupportUnavailable:
            return "Application Support directory unavailable."
        }
    }
}

private enum ClientConfigBootstrap {
    static func ensure(serverHost: String,
                       serverPort: String,
                       useTLS: Bool) throws -> String {
        guard let appSupport = FileManager.default.urls(for: .applicationSupportDirectory,
                                                        in: .userDomainMask).first else {
            throw ClientBootstrapError.appSupportUnavailable
        }
        let baseDir = appSupport.appendingPathComponent("mi_e2ee_ios_chat",
                                                        isDirectory: true)
        let dataDir = baseDir.appendingPathComponent("data", isDirectory: true)
        let stateDir = dataDir.appendingPathComponent("e2ee_state", isDirectory: true)
        let trustStore = baseDir.appendingPathComponent("server_trust.ini")
        let configPath = baseDir.appendingPathComponent("client_config.ini")

        try FileManager.default.createDirectory(at: baseDir,
                                                withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: dataDir,
                                                withIntermediateDirectories: true)
        try FileManager.default.createDirectory(at: stateDir,
                                                withIntermediateDirectories: true)

        if !FileManager.default.fileExists(atPath: trustStore.path) {
            try Data().write(to: trustStore, options: .atomic)
        }

        let normalizedHost = serverHost.trimmingCharacters(in: .whitespacesAndNewlines)
        let normalizedPort = serverPort.trimmingCharacters(in: .whitespacesAndNewlines)
        let config = """
        [client]
        server_ip=\(normalizedHost.isEmpty ? "127.0.0.1" : normalizedHost)
        server_port=\(normalizedPort.isEmpty ? "9000" : normalizedPort)
        use_tls=\(useTLS ? 1 : 0)
        trust_store=\(trustStore.path)

        [proxy]
        type=none
        host=
        port=0
        username=
        password=

        [device_sync]
        enabled=0
        role=primary
        key_path=\(stateDir.appendingPathComponent("device_sync_key.bin").path)
        """
        try config.write(to: configPath, atomically: true, encoding: .utf8)
        setenv("MI_E2EE_DATA_DIR", dataDir.path, 1)
        return configPath.path
    }
}

func stringValue(_ value: Any?) -> String {
    if let text = value as? String {
        return text
    }
    if let number = value as? NSNumber {
        return number.stringValue
    }
    return ""
}

func boolValue(_ value: Any?) -> Bool {
    if let flag = value as? Bool {
        return flag
    }
    if let number = value as? NSNumber {
        return number.boolValue
    }
    return false
}

func uint64Value(_ value: Any?) -> UInt64 {
    if let number = value as? NSNumber {
        return number.uint64Value
    }
    if let text = value as? String, let parsed = UInt64(text) {
        return parsed
    }
    return 0
}

func dictionaryArray(_ value: Any?) -> [[String: Any]] {
    if let array = value as? [[String: Any]] {
        return array
    }
    if let rawArray = value as? [Any] {
        return rawArray.compactMap { $0 as? [String: Any] }
    }
    if let nsArray = value as? NSArray {
        return nsArray.compactMap { $0 as? [String: Any] }
    }
    return []
}

@MainActor
final class ClientWorkspaceStore: ObservableObject {
    @Published var serverHost: String = "127.0.0.1"
    @Published var serverPort: String = "9000"
    @Published var useTLS: Bool = true
    @Published var username: String = ""
    @Published var password: String = ""
    @Published var rootCode: String = ""
    @Published var draft: String = ""
    @Published var statusText: String = "Preparing client..."
    @Published var lastError: String = ""
    @Published var configPath: String = ""
    @Published var selectedConversationID: String = ""
    @Published private(set) var isReady: Bool = false
    @Published private(set) var isLoggedIn: Bool = false
    @Published private(set) var remoteOK: Bool = false
    @Published private(set) var deviceDisplayID: String = ""
    @Published private(set) var conversations: [ClientConversation] = []
    @Published private(set) var messagesByConversation: [String: [ClientMessage]] = [:]
    @Published private(set) var deviceSummaries: [String] = []

    private let bridge = MIClientBridge()
    private var pollTimer: Timer?

    init() {
        if ScreenshotScenario.current != .none {
            loadScreenshotFixture()
            return
        }
        configureClient(resetSelection: true)
    }

    deinit {
        pollTimer?.invalidate()
        bridge.close()
    }

    var currentMessages: [ClientMessage] {
        messagesByConversation[selectedConversationID] ?? []
    }

    var currentConversationTitle: String {
        conversations.first(where: { $0.id == selectedConversationID })?.title ?? "No chat selected"
    }

    var contactConversations: [ClientConversation] {
        conversations.filter { !$0.isGroup }
    }

    var primaryConversation: ClientConversation? {
        let preferredID = selectedConversationID.isEmpty ? conversations.first?.id : selectedConversationID
        if let preferredID {
            return conversations.first(where: { $0.id == preferredID })
        }
        return conversations.first
    }

    func latestMessage(for conversationID: String) -> ClientMessage? {
        messagesByConversation[conversationID]?.max(by: { lhs, rhs in
            lhs.timestampMS < rhs.timestampMS
        })
    }

    func configureClient(resetSelection: Bool) {
        do {
            configPath = try ClientConfigBootstrap.ensure(serverHost: serverHost,
                                                         serverPort: serverPort,
                                                         useTLS: useTLS)
            bridge.close()
            if !bridge.open(withConfigPath: configPath) {
                lastError = bridge.lastCreateError
                statusText = lastError.isEmpty ? "Client initialization failed." : lastError
            } else {
                lastError = ""
                statusText = "Client ready."
            }
            refreshBridgeState()
            refreshDevices()
            if resetSelection {
                selectedConversationID = ""
            }
        } catch {
            lastError = error.localizedDescription
            statusText = error.localizedDescription
        }
    }

    func registerAccount() {
        configureClient(resetSelection: false)
        guard bridge.isReady else {
            statusText = "Client bridge unavailable."
            return
        }
        let ok = bridge.createAccount(withUsername: username, password: password)
        if ok {
            statusText = "Account created. Signing in..."
            signIn()
        } else {
            lastError = bridge.lastError
            statusText = lastError.isEmpty ? "Account creation failed." : lastError
        }
    }

    func signIn() {
        configureClient(resetSelection: false)
        guard bridge.isReady else {
            statusText = "Client bridge unavailable."
            return
        }
        let trimmedRoot = rootCode.trimmingCharacters(in: .whitespacesAndNewlines)
        let ok = trimmedRoot.isEmpty
            ? bridge.login(withUsername: username, password: password)
            : bridge.login(withUsername: username, password: password, rootCode: trimmedRoot)
        if ok {
            _ = bridge.publishPrekeys()
            refreshBridgeState()
            rebuildConversations()
            refreshDevices()
            loadSelectedConversation()
            startPolling()
            statusText = remoteOK ? "Signed in with secure session." : "Signed in."
        } else {
            refreshBridgeState()
            statusText = lastError.isEmpty ? "Sign in failed." : lastError
        }
    }

    func signOut() {
        stopPolling()
        bridge.close()
        isLoggedIn = false
        isReady = false
        remoteOK = false
        deviceDisplayID = ""
        conversations = []
        messagesByConversation = [:]
        deviceSummaries = []
        selectedConversationID = ""
        draft = ""
        statusText = "Signed out."
        configureClient(resetSelection: true)
    }

    func selectConversation(_ id: String) {
        guard !id.isEmpty else {
            return
        }
        selectedConversationID = id
        loadSelectedConversation()
    }

    func sendDraft() {
        let text = draft.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else {
            return
        }
        guard let conversation = conversations.first(where: { $0.id == selectedConversationID }) else {
            statusText = "Choose a conversation first."
            return
        }
        let messageID: String?
        if conversation.isGroup {
            messageID = bridge.sendGroupText(text, groupID: conversation.id)
        } else {
            messageID = bridge.sendPrivateText(text, toPeerUsername: conversation.id)
        }
        guard let sentID = messageID, !sentID.isEmpty else {
            lastError = bridge.lastError
            statusText = lastError.isEmpty ? "Send failed." : lastError
            return
        }
        appendMessage(
            ClientMessage(id: sentID,
                          conversationID: conversation.id,
                          sender: username,
                          text: text,
                          outgoing: true,
                          timestampMS: UInt64(Date().timeIntervalSince1970 * 1000))
        )
        draft = ""
        statusText = "Message sent."
    }

    func refreshNow() {
        tick()
        loadSelectedConversation()
    }

    private func startPolling() {
        stopPolling()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.tick()
            }
        }
        if let pollTimer {
            RunLoop.main.add(pollTimer, forMode: .common)
        }
    }

    private func stopPolling() {
        pollTimer?.invalidate()
        pollTimer = nil
    }

    private func tick() {
        guard bridge.isReady else {
            refreshBridgeState()
            return
        }
        let events = dictionaryArray(bridge.pollEvents(withMaxCount: 64, waitMS: 0))
        if !events.isEmpty {
            ingest(events: events)
        }
        refreshBridgeState()
        rebuildConversations()
        refreshDevices()
    }

    private func refreshBridgeState() {
        isReady = bridge.isReady
        isLoggedIn = !bridge.token.isEmpty
        remoteOK = bridge.remoteOK
        deviceDisplayID = bridge.deviceDisplayID
        if bridge.lastError.isEmpty {
            lastError = bridge.lastCreateError
        } else {
            lastError = bridge.lastError
        }
        if isLoggedIn {
            if remoteOK {
                statusText = "Secure session ready."
            } else if !bridge.remoteError.isEmpty {
                statusText = bridge.remoteError
            }
        }
    }

    private func refreshDevices() {
        let list = dictionaryArray(bridge.listDevices())
        deviceSummaries = list.map {
            let display = stringValue($0["displayID"])
            let deviceID = stringValue($0["deviceID"])
            if display.isEmpty {
                return deviceID
            }
            return "\(display) · \(deviceID)"
        }
    }

    private func rebuildConversations() {
        var merged: [String: ClientConversation] = [:]
        let friends = dictionaryArray(bridge.listFriends())
        for friend in friends {
            let id = stringValue(friend["username"])
            guard !id.isEmpty else {
                continue
            }
            let title = stringValue(friend["remark"]).isEmpty
                ? id
                : stringValue(friend["remark"])
            merged[id] = ClientConversation(id: id,
                                            title: title,
                                            subtitle: id,
                                            isGroup: false)
        }
        for conversationID in messagesByConversation.keys {
            if merged[conversationID] == nil {
                merged[conversationID] = ClientConversation(id: conversationID,
                                                            title: conversationID,
                                                            subtitle: "Recent activity",
                                                            isGroup: false)
            }
        }
        conversations = merged.values.sorted {
            $0.title.localizedCaseInsensitiveCompare($1.title) == .orderedAscending
        }
        if selectedConversationID.isEmpty, let first = conversations.first {
            selectedConversationID = first.id
        }
    }

    private func loadSelectedConversation() {
        guard !selectedConversationID.isEmpty else {
            return
        }
        let isGroup = conversations.first(where: { $0.id == selectedConversationID })?.isGroup ?? false
        let history = dictionaryArray(
            bridge.loadHistory(forConversationID: selectedConversationID,
                               isGroup: isGroup,
                               limit: 64)
        )
        guard !history.isEmpty else {
            return
        }
        var mapped: [ClientMessage] = []
        for item in history {
            let messageID = stringValue(item["messageID"])
            let text = resolvedText(from: item)
            let sender = stringValue(item["sender"])
            let timestamp = uint64Value(item["timestampMS"])
            mapped.append(
                ClientMessage(id: messageID.isEmpty ? UUID().uuidString : messageID,
                              conversationID: selectedConversationID,
                              sender: sender.isEmpty ? selectedConversationID : sender,
                              text: text,
                              outgoing: boolValue(item["outgoing"]),
                              timestampMS: timestamp)
            )
        }
        mapped.sort { $0.timestampMS < $1.timestampMS }
        messagesByConversation[selectedConversationID] = deduplicated(mapped)
    }

    private func ingest(events: [[String: Any]]) {
        for event in events {
            let conversationID = resolvedConversationID(from: event)
            guard !conversationID.isEmpty else {
                continue
            }
            if selectedConversationID.isEmpty {
                selectedConversationID = conversationID
            }
            let messageID = stringValue(event["messageID"])
            let sender = stringValue(event["sender"]).isEmpty
                ? conversationID
                : stringValue(event["sender"])
            appendMessage(
                ClientMessage(id: messageID.isEmpty ? UUID().uuidString : messageID,
                              conversationID: conversationID,
                              sender: sender,
                              text: resolvedText(from: event),
                              outgoing: boolValue(event["outgoing"]),
                              timestampMS: uint64Value(event["timestampMS"]))
            )
        }
    }

    private func appendMessage(_ message: ClientMessage) {
        var current = messagesByConversation[message.conversationID] ?? []
        if current.contains(where: { $0.id == message.id }) {
            return
        }
        current.append(message)
        current.sort { $0.timestampMS < $1.timestampMS }
        messagesByConversation[message.conversationID] = current
    }

    private func deduplicated(_ messages: [ClientMessage]) -> [ClientMessage] {
        var seen: Set<String> = []
        return messages.filter { message in
            if seen.contains(message.id) {
                return false
            }
            seen.insert(message.id)
            return true
        }
    }

    private func resolvedConversationID(from dict: [String: Any]) -> String {
        let direct = stringValue(dict["conversationID"])
        if !direct.isEmpty {
            return direct
        }
        let groupID = stringValue(dict["groupID"])
        if !groupID.isEmpty {
            return groupID
        }
        let peer = stringValue(dict["peer"])
        if !peer.isEmpty {
            return peer
        }
        return stringValue(dict["sender"])
    }

    private func resolvedText(from dict: [String: Any]) -> String {
        let text = stringValue(dict["text"])
        if !text.isEmpty {
            return text
        }
        let fileName = stringValue(dict["fileName"])
        if !fileName.isEmpty {
            return "[File] \(fileName)"
        }
        let messageType = stringValue(dict["type"])
        return messageType.isEmpty ? "Event received" : "Event \(messageType)"
    }

    private func loadScreenshotFixture() {
        isReady = true
        isLoggedIn = true
        remoteOK = true
        deviceDisplayID = "ios-sim-01"
        serverHost = "secure-gateway.internal"
        serverPort = "9000"
        username = "aster"
        draft = "Meeting notes are encrypted and ready to send."
        statusText = "Screenshot fixture loaded."
        lastError = ""
        configPath = "screenshot://fixture"
        let now = UInt64(Date().timeIntervalSince1970 * 1000)
        let fixtureConversations = [
            ClientConversation(
                id: "c-aster",
                title: "Aster Stone",
                subtitle: "Security review at 10:30",
                isGroup: false
            ),
            ClientConversation(
                id: "g-threat",
                title: "Threat Guild",
                subtitle: "Rotation completed for 12 members",
                isGroup: true
            ),
            ClientConversation(
                id: "c-mira",
                title: "Mira Chen",
                subtitle: "Uploaded the audit package",
                isGroup: false
            ),
            ClientConversation(
                id: "c-ops",
                title: "Ops Sync",
                subtitle: "Queue cap increased to 512",
                isGroup: false
            ),
            ClientConversation(
                id: "c-rhea",
                title: "Rhea North",
                subtitle: "Typing indicator verified",
                isGroup: false
            ),
            ClientConversation(
                id: "g-platform",
                title: "Platform",
                subtitle: "API33 smoke gate is green",
                isGroup: true
            )
        ]
        conversations = fixtureConversations
        messagesByConversation = [
            "c-aster": [
                ClientMessage(
                    id: "m1",
                    conversationID: "c-aster",
                    sender: "Aster Stone",
                    text: "The secure handoff build is ready for review.",
                    outgoing: false,
                    timestampMS: now - 600_000
                ),
                ClientMessage(
                    id: "m2",
                    conversationID: "c-aster",
                    sender: "You",
                    text: "Send me the final screenshot bundle after CI passes.",
                    outgoing: true,
                    timestampMS: now - 420_000
                ),
                ClientMessage(
                    id: "m2b",
                    conversationID: "c-aster",
                    sender: "You",
                    text: "I want the desktop, Android, and iOS shells to feel consistent.",
                    outgoing: true,
                    timestampMS: now - 360_000
                ),
                ClientMessage(
                    id: "m3",
                    conversationID: "c-aster",
                    sender: "Aster Stone",
                    text: "Accepted. I will keep the screenshots attached to the release.",
                    outgoing: false,
                    timestampMS: now - 180_000
                ),
                ClientMessage(
                    id: "m4",
                    conversationID: "c-aster",
                    sender: "Aster Stone",
                    text: "I also tightened the settings hierarchy so Security Center sits under the main product shell.",
                    outgoing: false,
                    timestampMS: now - 120_000
                )
            ],
            "c-ops": [
                ClientMessage(
                    id: "o1",
                    conversationID: "c-ops",
                    sender: "Ops Sync",
                    text: "Release package uploaded and signed.",
                    outgoing: false,
                    timestampMS: now - 300_000
                )
            ],
            "c-rhea": [
                ClientMessage(
                    id: "r1",
                    conversationID: "c-rhea",
                    sender: "Rhea North",
                    text: "Screenshot pass is green on Android.",
                    outgoing: false,
                    timestampMS: now - 240_000
                )
            ],
            "g-platform": [
                ClientMessage(
                    id: "p1",
                    conversationID: "g-platform",
                    sender: "Platform",
                    text: "Smoke gate passed on API33 with the new fixture set.",
                    outgoing: false,
                    timestampMS: now - 120_000
                )
            ],
            "g-threat": [
                ClientMessage(
                    id: "g1",
                    conversationID: "g-threat",
                    sender: "System",
                    text: "Sender key rotation completed successfully.",
                    outgoing: false,
                    timestampMS: now - 1_200_000
                )
            ],
            "c-mira": [
                ClientMessage(
                    id: "m4",
                    conversationID: "c-mira",
                    sender: "Mira Chen",
                    text: "Uploaded the audit package and linked device report.",
                    outgoing: false,
                    timestampMS: now - 900_000
                )
            ]
        ]
        deviceSummaries = [
            "iPhone 15 Pro · Secure session active",
            "Windows Workstation · Last seen 2m ago"
        ]
        selectedConversationID = "c-aster"
    }
}

private enum AppTab: Hashable {
    case chats
    case contacts
    case calls
    case settings
}

struct AppShell: View {
    private let screenshotScenario: ScreenshotScenario
    @StateObject private var rootAuthStore = RootAuthStore()
    @StateObject private var clientStore = ClientWorkspaceStore()
    @State private var selectedTab: AppTab

    private var shellBackground: some View {
        SecureSceneBackground()
            .background(SecurePalette.backgroundBottom)
    }

    init() {
        let scenario = ScreenshotScenario.current
        screenshotScenario = scenario
        _selectedTab = State(initialValue: scenario == .security ? .settings : .chats)
        Self.configureTabBarAppearance()
        Self.configureNavigationBarAppearance()
    }

    var body: some View {
        ZStack(alignment: .top) {
            SecureWindowConfigurator()
            shellBackground
                .ignoresSafeArea()

            TabView(selection: $selectedTab) {
                NavigationStack {
                    if screenshotScenario == .detail,
                       let conversation = clientStore.primaryConversation {
                        ClientConversationDetailView(store: clientStore, conversation: conversation)
                    } else {
                        ClientWorkspaceView(store: clientStore)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                .background(shellBackground)
                .tabItem {
                    Label("Chats", systemImage: "message.fill")
                }
                .tag(AppTab.chats)

                NavigationStack {
                    ContactsHomeView(store: clientStore)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                .background(shellBackground)
                .tabItem {
                    Label("Contacts", systemImage: "person.2.fill")
                }
                .tag(AppTab.contacts)

                NavigationStack {
                    CallsHomeView(store: clientStore)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                .background(shellBackground)
                .tabItem {
                    Label("Calls", systemImage: "phone.fill")
                }
                .tag(AppTab.calls)

                NavigationStack {
                    SettingsHomeView(clientStore: clientStore, rootAuthStore: rootAuthStore)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                .background(shellBackground)
                .tabItem {
                    Label("Settings", systemImage: "gearshape.fill")
                }
                .tag(AppTab.settings)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .background(shellBackground)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(shellBackground)
        .toolbarBackground(SecurePalette.backgroundTop.opacity(0.98), for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarBackground(SecurePalette.backgroundBottom.opacity(0.98), for: .tabBar)
        .toolbarBackground(.visible, for: .tabBar)
        .toolbarColorScheme(.dark, for: .navigationBar)
        .tint(SecurePalette.accent)
        .preferredColorScheme(.dark)
    }

    private static func configureTabBarAppearance() {
        let appearance = UITabBarAppearance()
        appearance.configureWithOpaqueBackground()
        appearance.backgroundEffect = nil
        appearance.backgroundColor = UIColor(SecurePalette.backgroundBottom)
        appearance.shadowColor = UIColor(SecurePalette.border.opacity(0.55))

        let selectedColor = UIColor(SecurePalette.accent)
        let normalColor = UIColor.white.withAlphaComponent(0.84)
        let selectedTextAttributes: [NSAttributedString.Key: Any] = [.foregroundColor: selectedColor]
        let normalTextAttributes: [NSAttributedString.Key: Any] = [.foregroundColor: normalColor]

        for layout in [appearance.stackedLayoutAppearance,
                       appearance.inlineLayoutAppearance,
                       appearance.compactInlineLayoutAppearance] {
            layout.selected.iconColor = selectedColor
            layout.selected.titleTextAttributes = selectedTextAttributes
            layout.normal.iconColor = normalColor
            layout.normal.titleTextAttributes = normalTextAttributes
        }

        let tabBar = UITabBar.appearance()
        tabBar.standardAppearance = appearance
        tabBar.unselectedItemTintColor = normalColor
        tabBar.isTranslucent = false
        if #available(iOS 15.0, *) {
            tabBar.scrollEdgeAppearance = appearance
        }
    }

    private static func configureNavigationBarAppearance() {
        let appearance = UINavigationBarAppearance()
        appearance.configureWithOpaqueBackground()
        appearance.backgroundEffect = nil
        appearance.backgroundColor = UIColor(SecurePalette.backgroundTop.opacity(0.96))
        appearance.shadowColor = UIColor(SecurePalette.border)
        appearance.titleTextAttributes = [
            .foregroundColor: UIColor(SecurePalette.textPrimary)
        ]
        appearance.largeTitleTextAttributes = [
            .foregroundColor: UIColor(SecurePalette.textPrimary)
        ]

        let navigationBar = UINavigationBar.appearance()
        navigationBar.standardAppearance = appearance
        navigationBar.scrollEdgeAppearance = appearance
        navigationBar.compactAppearance = appearance
        navigationBar.tintColor = UIColor(SecurePalette.accent)
    }
}
