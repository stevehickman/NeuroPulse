import SwiftUI
import NeurOneShared

// The iOS host of the shared app: a SwiftUI shell around one Kotlin view controller. All screens and
// all flow logic are in app/NeurOneUI/shared; this file only hosts them.
@main
struct NeurOneUIApp: App {
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
