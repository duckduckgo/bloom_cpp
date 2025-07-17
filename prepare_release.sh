#!/bin/bash

set -e

mute=">/dev/null 2>&1"
if [[ "$1" == "-v" ]]; then
	mute=
fi

cwd="$(dirname "${BASH_SOURCE[0]}")"
workdir="$(mktemp -d)"
mkdir -p "${workdir}/Logs"
# trap 'rm -rf "$workdir"' EXIT

print_usage_and_exit() {
	cat <<- EOF
	Usage:
	  $ $(basename "$0") [-v] [-h] [--skip-tests] [--dry-run] [--force-branch] [<version>]

	Options:
	 -h              Show this message
	 -v              Verbose output
	 --skip-tests    Skip running tests before building
	 --dry-run       Prepare release but skip git operations (commit, tag, push)
	 --force-branch  Allow running from any branch (bypasses main branch requirement)
	EOF

	exit 1
}

validate_version() {
	if [[ -n "$new_version" ]]; then
		if ! [[ "$new_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
			echo "❌ Error: Invalid version format '$new_version'"
			echo "Version must follow semantic versioning format: x.y.z (e.g., 1.2.3)"
			exit 1
		fi
	fi
}

read_command_line_arguments() {
	# Process all arguments
	while [[ $# -gt 0 ]]; do
		case "$1" in
			-h)
				print_usage_and_exit
				;;
			-v)
				mute=
				shift
				;;
			--skip-tests)
				skip_tests=1
				shift
				;;
			--dry-run)
				dry_run=1
				shift
				;;
			--force-branch)
				force_branch=1
				shift
				;;
			-*)
				echo "Unknown option: $1"
				print_usage_and_exit
				;;
			*)
				# This should be the version argument
				if [[ -z "$new_version" ]]; then
					new_version="$1"
					force_release=1
				else
					echo "Error: Multiple version arguments provided: '$new_version' and '$1'"
					exit 1
				fi
				shift
				;;
		esac
	done
	
	# Validate version format if provided
	validate_version
}

update_readme() {
	current_version="$(git describe --tags --abbrev=0 2>/dev/null || echo "0.0.0")"
	
	export new_version

	if [[ -z "$force_release" ]]; then
		cat <<- EOF

		BloomFilter C++ current version: ${current_version}
		EOF

		while [[ -z "$new_version" ]]; do
			read -rp "Input BloomFilter desired version number (x.y.z): " new_version < /dev/tty
			# Validate the entered version
			if ! [[ "$new_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
				echo "❌ Invalid version format. Please use semantic versioning format: x.y.z (e.g., 1.2.3)"
				new_version=""
			fi
		done
	fi

	echo "Target version: ${new_version}"
	echo ""
}

build_static_library() {
	local platform=$1
	local arch=$2
	local build_dir="${workdir}/build-${platform}-${arch}"
	local install_dir="${workdir}/install-${platform}-${arch}"
	local log_file="${workdir}/Logs/BloomFilter-${platform}-${arch}.log"

	printf '%s' "  * Building static library for ${platform} ${arch} ... "

	# Resolve absolute paths BEFORE changing directories
	local abs_cwd="$(cd "$cwd" && pwd)"
	local source_file="${abs_cwd}/src/BloomFilter.cpp"

	# Ensure directories exist
	mkdir -p "$build_dir" "$install_dir" "${workdir}/Logs"

	cd "$build_dir"

	# For now, let's build a simple static library using direct compilation
	local compiler_flags="-std=c++14 -O2 -fPIC"
	local output_lib="${install_dir}/libBloomFilter.a"

	case "$platform" in
		"macOS")
			local sdk_path="$(xcrun --sdk macosx --show-sdk-path)"
			compiler_flags+=" -isysroot $sdk_path -mmacosx-version-min=10.15"
			if [[ "$arch" == "arm64" ]]; then
				compiler_flags+=" -arch arm64"
			else
				compiler_flags+=" -arch x86_64"
			fi
			;;
		"iOS")
			local sdk_path="$(xcrun --sdk iphoneos --show-sdk-path)"
			compiler_flags+=" -isysroot $sdk_path -mios-version-min=14.0 -arch $arch"
			# iOS specific flags
			compiler_flags+=" -fembed-bitcode"
			;;
		"iOS-Simulator")
			local sdk_path="$(xcrun --sdk iphonesimulator --show-sdk-path)"
			compiler_flags+=" -isysroot $sdk_path -mios-simulator-version-min=14.0 -arch $arch"
			;;
	esac

	mkdir -p "$(dirname "$output_lib")"

	# Create log file first
	
	echo "Building static library for ${platform} ${arch}" > "$log_file"
	echo "Compiler flags: $compiler_flags" >> "$log_file"
	echo "Source file: $source_file" >> "$log_file"
	echo "Output library: $output_lib" >> "$log_file"
	echo "Working directory: $(pwd)" >> "$log_file"
	echo "---" >> "$log_file"

	if clang++ $compiler_flags -c "$source_file" -o BloomFilter.o >>"$log_file" 2>&1 && \
		ar rcs "$output_lib" BloomFilter.o >>"$log_file" 2>&1; then
		echo "✅"
	else
		echo "❌"
		echo "Failed to build static library for ${platform} ${arch}. See log file at ${log_file}"
		return 1
	fi

	cd - >/dev/null
}

create_framework() {
	local platform=$1
	local arch=$2
	local install_dir="${workdir}/install-${platform}-${arch}"
	local framework_dir="${workdir}/frameworks/${platform}-${arch}/BloomFilter.framework"
	local framework_name="BloomFilter"

	printf '%s' "  * Creating framework for ${platform} ${arch} ... "

	mkdir -p "$framework_dir"

	# Create framework structure based on platform
	if [[ "$platform" == "macOS" ]]; then
		# macOS uses deep bundle structure
		mkdir -p "$framework_dir/Versions/A/Headers"
		mkdir -p "$framework_dir/Versions/A/Modules"
		mkdir -p "$framework_dir/Versions/A/Resources"
		
		# Create symlinks for deep bundle structure
		ln -sf "A" "$framework_dir/Versions/Current"
		ln -sf "Versions/Current/Headers" "$framework_dir/Headers"
		ln -sf "Versions/Current/Modules" "$framework_dir/Modules"
		ln -sf "Versions/Current/Resources" "$framework_dir/Resources"
		ln -sf "Versions/Current/$framework_name" "$framework_dir/$framework_name"
		
		# Copy library
		cp "$install_dir/libBloomFilter.a" "$framework_dir/Versions/A/$framework_name"
		
		# Copy headers
		local abs_cwd="$(cd "$cwd" && pwd)"
		cp "$abs_cwd/src/BloomFilter.hpp" "$framework_dir/Versions/A/Headers/"
	else
		# iOS uses shallow bundle structure
		mkdir -p "$framework_dir/Headers"
		mkdir -p "$framework_dir/Modules"
		
		# Copy library
		cp "$install_dir/libBloomFilter.a" "$framework_dir/$framework_name"
		
		# Copy headers
		local abs_cwd="$(cd "$cwd" && pwd)"
		cp "$abs_cwd/src/BloomFilter.hpp" "$framework_dir/Headers/"
	fi

	# Create Info.plist with platform-specific settings
	local min_os_version supported_platforms info_plist_path headers_path modules_path
	case "$platform" in
		"macOS")
			min_os_version="10.15"
			supported_platforms="<string>MacOSX</string>"
			info_plist_path="$framework_dir/Versions/A/Resources/Info.plist"
			headers_path="$framework_dir/Versions/A/Headers"
			modules_path="$framework_dir/Versions/A/Modules"
			;;
		"iOS")
			min_os_version="14.0"
			supported_platforms="<string>iPhoneOS</string>"
			info_plist_path="$framework_dir/Info.plist"
			headers_path="$framework_dir/Headers"
			modules_path="$framework_dir/Modules"
			;;
		"iOS-Simulator")
			min_os_version="14.0"
			supported_platforms="<string>iPhoneSimulator</string>"
			info_plist_path="$framework_dir/Info.plist"
			headers_path="$framework_dir/Headers"
			modules_path="$framework_dir/Modules"
			;;
	esac
	
	cat > "$info_plist_path" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>$framework_name</string>
	<key>CFBundleIdentifier</key>
	<string>com.duckduckgo.BloomFilter</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>$framework_name</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>$new_version</string>
	<key>CFBundleVersion</key>
	<string>$new_version</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>MinimumOSVersion</key>
	<string>$min_os_version</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		$supported_platforms
	</array>
</dict>
</plist>
EOF

	# Create an umbrella header for better compatibility
	cat > "$headers_path/BloomFilter.h" << EOF
#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include "BloomFilter.hpp"

#endif /* BLOOMFILTER_H */
EOF

	# Create module.modulemap
	cat > "$modules_path/module.modulemap" << EOF
framework module $framework_name {
    umbrella header "BloomFilter.h"
    export *
    module * { export * }
}
EOF

	echo "✅"
}

build_all_platforms() {
	echo ""
	echo "Building BloomFilter library for all platforms ⚙️"

	rm -rf "${workdir}/build-*" "${workdir}/install-*" "${workdir}/frameworks"
	mkdir -p "${workdir}/frameworks"

	# Build for macOS (both architectures)
	build_static_library "macOS" "x86_64"
	build_static_library "macOS" "arm64"

	# Build for iOS device (arm64 only)
	build_static_library "iOS" "arm64"

	# Build for iOS Simulator (both architectures)
	build_static_library "iOS-Simulator" "x86_64"
	build_static_library "iOS-Simulator" "arm64"

	# Create universal macOS library
	printf '%s' "  * Creating universal macOS library ... "
	local universal_lib="${workdir}/install-macOS-universal/libBloomFilter.a"
	mkdir -p "$(dirname "$universal_lib")"
	if lipo -create \
		"${workdir}/install-macOS-x86_64/libBloomFilter.a" \
		"${workdir}/install-macOS-arm64/libBloomFilter.a" \
		-output "$universal_lib" 2>/dev/null; then
		echo "✅"
	else
		echo "❌"
		echo "Failed to create universal macOS library"
		return 1
	fi

	# Create universal iOS Simulator library
	printf '%s' "  * Creating universal iOS Simulator library ... "
	local universal_ios_sim_lib="${workdir}/install-iOS-Simulator-universal/libBloomFilter.a"
	mkdir -p "$(dirname "$universal_ios_sim_lib")"
	if lipo -create \
		"${workdir}/install-iOS-Simulator-x86_64/libBloomFilter.a" \
		"${workdir}/install-iOS-Simulator-arm64/libBloomFilter.a" \
		-output "$universal_ios_sim_lib" 2>/dev/null; then
		echo "✅"
	else
		echo "❌"
		echo "Failed to create universal iOS Simulator library"
		return 1
	fi

	# Create frameworks
	create_framework "macOS" "universal"
	create_framework "iOS" "arm64"
	create_framework "iOS-Simulator" "universal"
}

build_xcframework() {
	local xcframework="${workdir}/BloomFilter.xcframework"
	xcframework_zip="${workdir}/BloomFilter.xcframework.zip"

	echo ""
	echo "Building XCFramework ⚙️"

	printf '%s' "Creating XCFramework ... "
	rm -rf "$xcframework"
	
	# Include all platform frameworks
	xcodebuild -create-xcframework \
		-framework "${workdir}/frameworks/macOS-universal/BloomFilter.framework" \
		-framework "${workdir}/frameworks/iOS-arm64/BloomFilter.framework" \
		-framework "${workdir}/frameworks/iOS-Simulator-universal/BloomFilter.framework" \
		-output "$xcframework" >/dev/null 2>&1
	echo "✅"
	
	printf '%s' "Compressing XCFramework ... "
	rm -rf "$xcframework_zip"
	ditto -c -k --keepParent "$xcframework" "$xcframework_zip"
	echo "✅"
}

compute_checksum() {
	printf '%s' "Computing XCFramework checksum ... "
	export checksum
	checksum=$(swift package compute-checksum "$xcframework_zip")
	
	echo "✅"
	echo "📦 XCFramework URL: https://github.com/duckduckgo/bloom_cpp/releases/download/${new_version}/BloomFilter.xcframework.zip"
	echo "🔢 Checksum: ${checksum}"
}

make_release() {
	if [[ "$dry_run" == "1" ]]; then
		echo "Preparing ${new_version} release (DRY RUN) ... 🚢"
	else
		echo "Preparing ${new_version} release ... 🚢"
	fi

	local commit_message="BloomFilter C++ ${new_version}"

	if [[ "$dry_run" == "1" ]]; then
		echo "DRY RUN: Would execute these git commands:"
		echo "  git tag -m \"$commit_message\" \"$new_version\""
		echo "  git push origin main"
		echo "  git push origin \"$new_version\""
		echo ""
		echo "DRY RUN: Would create GitHub release:"
		echo "  gh release create \"$new_version\" --generate-notes \"${xcframework_zip}\" --repo duckduckgo/bloom_cpp"
	else
		# Execute git operations
		git tag -m "$commit_message" "$new_version"
		git push origin main
		git push origin "$new_version"

		gh release create "$new_version" --generate-notes "${xcframework_zip}" --repo duckduckgo/bloom_cpp
		
		echo "✅ Git operations completed successfully."
		echo "✅ GitHub release created."
	fi

	cat <<- EOF

	🎉 Release artifacts prepared at ${workdir}
	📦 XCFramework: ${xcframework_zip}
	🔢 Checksum: ${checksum}
	
	📝 To add this package as a dependency:
	   1. In Xcode: File → Add Package Dependencies
	   2. Enter URL: https://github.com/duckduckgo/bloom_cpp
	   3. Or add to Package.swift manually:
	   
	      .package(url: "https://github.com/duckduckgo/bloom_cpp", from: "${new_version}")
	
	🔄 To update Package.swift for binary distribution:
	   Replace the existing target with:
	   
	      .binaryTarget(
	          name: "BloomFilter",
	          url: "https://github.com/duckduckgo/bloom_cpp/releases/download/${new_version}/BloomFilter.xcframework.zip",
	          checksum: "${checksum}"
	      )
	
	💡 Usage in your code:
	   Swift: import BloomFilter
	   C++:   #include <BloomFilter/BloomFilter.h>
	   ObjC++: #import <BloomFilter/BloomFilter.h>
	EOF

	if [[ "$dry_run" == "1" ]]; then
		cat <<- EOF
		
		🧪 DRY RUN completed successfully!
		    No git operations were performed.
		    Remove --dry-run flag to perform actual release.
		EOF
	fi
}

validate_branch() {
	printf '%s' "Validating git branch ... "
	
	local current_branch=$(git branch --show-current 2>/dev/null)
	
	if [[ -z "$current_branch" ]]; then
		echo "❌"
		echo "Error: Unable to determine current git branch. Are you in a git repository?"
		exit 1
	fi
	
	if [[ "$current_branch" != "main" ]]; then
		if [[ "$force_branch" == "1" ]]; then
			echo "⚠️  (on '$current_branch' branch, but --force-branch used)"
			echo "Warning: Running from '$current_branch' instead of main branch."
		else
			echo "❌"
			echo "Error: Current branch is '$current_branch', but releases must be made from 'main' branch."
			echo "Please switch to main branch: git checkout main"
			echo "Or use --force-branch to override this check."
			exit 1
		fi
	else
		echo "✅ (on main branch)"
	fi
}

run_tests() {
	if [[ "$skip_tests" == "1" ]]; then
		echo "Skipping tests (--skip-tests flag provided) ⚠️"
		return 0
	fi

	printf '%s' "Running tests ... "
	
	if [[ -f "${cwd}/run_test.sh" ]]; then
		if eval "${cwd}/run_test.sh" "$mute"; then
			echo "✅"
		else
			echo "❌"
			echo "Tests failed. Continuing with build anyway..."
			echo "⚠️  You may want to fix the tests before releasing."
		fi
	else
		echo "⚠️  (no run_test.sh found)"
	fi
}

main() {
	printf '%s\n' "Using directory at ${workdir}"

	read_command_line_arguments "$@"

	validate_branch
	update_readme
	run_tests
	build_all_platforms
	build_xcframework
	compute_checksum
	make_release
}

main "$@"
