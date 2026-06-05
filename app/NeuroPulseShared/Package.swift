// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "NeuroPulseShared",
    platforms: [
        .iOS(.v17),
        .watchOS(.v10),
    ],
    products: [
        .library(name: "NeuroPulseShared", targets: ["NeuroPulseShared"]),
    ],
    targets: [
        .target(
            name: "NeuroPulseShared",
            path: "Sources/NeuroPulseShared"
        ),
    ]
)
