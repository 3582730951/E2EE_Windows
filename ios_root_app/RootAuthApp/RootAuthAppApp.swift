import SwiftUI

@main
struct RootAuthAppApp: App {
    var body: some Scene {
        WindowGroup {
            ZStack {
                SecureSceneBackground()
                AppShell()
            }
        }
    }
}
