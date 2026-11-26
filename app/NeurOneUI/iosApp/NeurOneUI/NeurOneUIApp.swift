import SwiftUI
import NeurOneShared

// The iOS host of the shared app: a SwiftUI shell around one Kotlin view controller. All screens and
// all flow logic are in app/NeurOneUI/shared; this file only hosts them.
@main
struct NeurOneUIApp: App {
    // iOS refuses a background-task handler registered after launch finishes, so it is registered here, before any scene.
    init() {
        IosBackgroundBackupKt.registerBackgroundBackup()
    }

    // The scene's phase, for the app-switcher cover below.
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            ZStack {
                ComposeHost().ignoresSafeArea()
                // REQ-SW03-36 (d): iOS takes the app-switcher snapshot as the app leaves the foreground, and shows it to anyone who
                // opens the switcher. Every screen can hold health data (the recovery code, consent, the history, a session), so the
                // whole window is covered whenever the scene is not active, not one screen. iOS has no switch that keeps a window out
                // of a screenshot; this covers the snapshot only. An opaque cover rather than a blur: a blur leaves large text, such as
                // the recovery code, readable. Inactive counts too (the switcher opening, the system's Face ID prompt), because the
                // snapshot may be taken before the scene reaches the background.
                if scenePhase != .active {
                    Color(uiColor: .systemBackground).ignoresSafeArea()
                }
            }
        }
    }
}

struct ComposeHost: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UIViewController {
        MainViewControllerKt.MainViewController()
    }

    func updateUIViewController(_ uiViewController: UIViewController, context: Context) {}
}
