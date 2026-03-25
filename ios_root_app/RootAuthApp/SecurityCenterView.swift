import SwiftUI

struct SecurityCenterView: View {
    @ObservedObject var clientStore: ClientWorkspaceStore
    @ObservedObject var rootAuthStore: RootAuthStore

    var body: some View {
        ContentView(store: rootAuthStore, embeddedTitle: "Trust, devices, and root authorization")
            .navigationTitle("Security Center")
            .navigationBarTitleDisplayMode(.inline)
    }
}
