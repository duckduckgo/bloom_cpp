// swift-tools-version: 5.7
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "BloomFilter",
    platforms: [
        .iOS(.v14),
        .macOS(.v10_15)
    ],
    products: [
        .library(
            name: "BloomFilter",
            targets: ["BloomFilter"]
        ),
    ],
    targets: [
        .target(
            name: "BloomFilter",
            path: "src",
            sources: ["BloomFilter.cpp"],
            publicHeadersPath: "."
        )
    ],
    cxxLanguageStandard: .cxx11
)
