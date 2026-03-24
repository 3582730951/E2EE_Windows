import SwiftUI

struct SecurityCenterView: View {
    @ObservedObject var clientStore: ClientWorkspaceStore
    @ObservedObject var rootAuthStore: RootAuthStore

    var body: some View {
        ContentView(store: rootAuthStore)
            .safeAreaInset(edge: .top, spacing: 0) {
                VStack(spacing: 14) {
                    SecureSectionHeader(
                        eyebrow: "Security Center",
                        title: "Trust, devices, and root authorization",
                        detail: "Root Auth is now part of the main client product, not a separate top-level app mode."
                    )

                    HStack(spacing: 12) {
                        SecureMetricTile(
                            label: "Session",
                            value: clientStore.remoteOK ? "Verified" : "Attention needed",
                            icon: "lock.shield"
                        )
                        SecureMetricTile(
                            label: "Linked devices",
                            value: "\(clientStore.deviceSummaries.count)",
                            icon: "desktopcomputer.and.iphone"
                        )
                    }
                }
                .padding(.horizontal, 18)
                .padding(.top, 12)
                .padding(.bottom, 4)
                .background(
                    LinearGradient(
                        colors: [SecurePalette.backgroundTop.opacity(0.96),
                                 SecurePalette.backgroundBottom.opacity(0.88)],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                )
            }
        .navigationTitle("Security Center")
        .navigationBarTitleDisplayMode(.inline)
    }
}
