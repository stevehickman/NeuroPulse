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
            name: "NeuroPulseArgon2",
            path: "Sources/NeuroPulseArgon2",
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("phc/include"),
                .headerSearchPath("phc/src"),
                .define("ARGON2_NO_THREADS"),
                // Gate test-only KDF out of release binaries.
                // Swift #if DEBUG in Argon2Bridge.swift mirrors this guard.
                .define("NP_ARGON2_CUSTOM_ENABLED", .when(configuration: .debug)),
            ]
        ),
        .target(
            name: "NeuroPulseShared",
            dependencies: ["NeuroPulseArgon2"],
            path: "Sources/NeuroPulseShared"
        ),
    ]
)
