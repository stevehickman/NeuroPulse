// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "NeurOneShared",
    platforms: [
        .iOS(.v17),
        .watchOS(.v10),
    ],
    products: [
        .library(name: "NeurOneShared", targets: ["NeurOneShared"]),
    ],
    targets: [
        .target(
            name: "NeurOneShared",
            path: "Sources/NeurOneShared"
        ),
    ]
)
