// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "ArabicDemo",
    platforms: [.macOS(.v13)],
    targets: [
        .target(name: "CArabicRuntime"),
        .executableTarget(name: "ArabicDemo", dependencies: ["CArabicRuntime"]),
    ]
)
