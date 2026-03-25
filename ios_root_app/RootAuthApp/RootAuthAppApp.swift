import SwiftUI

@main
struct RootAuthAppApp: App {
    var body: some Scene {
        WindowGroup {
            ZStack(alignment: .top) {
                SecureSceneBackground()
                AppShell()
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .background(SecurePalette.backgroundBottom)
        }
    }
}
