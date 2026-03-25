import SwiftUI

@main
struct RootAuthAppApp: App {
    var body: some Scene {
        WindowGroup {
            GeometryReader { proxy in
                ZStack {
                    SecureSceneBackground()

                    AppShell()
                        .frame(width: proxy.size.width, height: proxy.size.height)
                }
                .frame(width: proxy.size.width, height: proxy.size.height)
                .background(SecurePalette.backgroundBottom)
            }
        }
    }
}
