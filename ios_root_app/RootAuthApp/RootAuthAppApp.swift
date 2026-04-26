import SwiftUI

@main
struct RootAuthAppApp: App {
    @Environment(\.scenePhase) private var scenePhase
    private let endpointThreatReport = EndpointThreatDetector.evaluate()

    var body: some Scene {
        WindowGroup {
            if endpointThreatReport.blocked {
                EndpointThreatBlockedView(report: endpointThreatReport)
                    .onAppear {
                        SecureClipboard.clear()
                    }
            } else {
                AppShell()
                    .onChange(of: scenePhase) { phase in
                        if phase != .active {
                            SecureClipboard.clear()
                        }
                    }
            }
        }
    }
}
