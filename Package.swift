// swift-tools-version:5.7
import PackageDescription

let package = Package(
    name: "BloomFilter",
    platforms: [
        .iOS("14.0"),
        .macOS("10.15")
    ],
    products: [
        .library(name: "BloomFilter", targets: ["BloomFilter"]),
    ],
    targets: [
        .target(
            name: "BloomFilter",
            path: ".",
            sources: ["BloomFilter.cpp"]),
    ],
    cxxLanguageStandard: .cxx11
)