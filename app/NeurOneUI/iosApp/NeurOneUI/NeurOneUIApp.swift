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

    var body: some Scene {
        WindowGroup {
            ComposeHost().ignoresSafeArea()
        }
    }
}

struct ComposeHost: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UIViewController {
        MainViewControllerKt.MainViewController()
    }

    func updateUIViewController(_ uiViewController: UIViewController, context: Context) {}
}
