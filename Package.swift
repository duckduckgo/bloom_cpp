// swift-tools-version:5.3
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription
import Foundation

let package = Package(
    name: "BloomFilter",
    platforms: [
        .iOS("13.0"),
        .macOS("10.15")
    ],
    products: [
        .library(name: "BloomFilter", targets: ["BloomFilter"]),
    ],
    dependencies: [
    ],
    targets: [
        .target(
            name: "BloomFilter",
            path: "src",
            resources: [
                .process("CMakeLists.txt")
            ]),
    ],
    cxxLanguageStandard: .cxx11
)
