#!/usr/bin/env bash
# Builds the shared NPPS core's C ABI (common/npps-ffi, OI-NPPS-CORE-01) as an XCFramework the iOS
# app links: app/ios/Frameworks/NeurOneNppsCore.xcframework, a git-ignored build output.
#
#   scripts/build-npps-xcframework.sh
#
# Slices: iOS device (arm64) and iOS simulator (arm64 + x86_64, lipo'd). Needs macOS with Xcode and a
# Rust toolchain; the three Apple targets are added with rustup if it is present. app/ios/project.yml's
# preGenCommand runs this before XcodeGen reads the tree, because XcodeGen builds the project from what
# is on disk and a framework that does not exist yet is silently left out of it.
set -euo pipefail

root="$(git rev-parse --show-toplevel)"
common="$root/common"
out="$root/app/ios/Frameworks/NeurOneNppsCore.xcframework"
export PATH="$HOME/.cargo/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"

if ! command -v cargo >/dev/null 2>&1; then
  echo "error: cargo not found. The iOS app links the Rust NPPS core; install Rust (https://rustup.rs)." >&2
  exit 1
fi

# Up to date: nothing the library is built from is newer than the framework.
if [ -d "$out" ] && [ -z "$(find "$common/npps-core" "$common/npps-ffi" "$common/npps/fields.json" \
      "$common/Cargo.toml" "$common/Cargo.lock" "$root/scripts/build-npps-xcframework.sh" \
      -type f -not -path '*/target/*' -newer "$out" 2>/dev/null | head -n 1)" ]; then
  echo "NeurOneNppsCore.xcframework is up to date"
  exit 0
fi

targets="aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios"
if command -v rustup >/dev/null 2>&1; then
  # shellcheck disable=SC2086
  rustup target add $targets
fi

# The deployment target of the Rust code must not be newer than the app's (project.yml: 17.0).
export IPHONEOS_DEPLOYMENT_TARGET=17.0
for t in $targets; do
  cargo build --release --locked --manifest-path "$common/Cargo.toml" -p neurone-npps-ffi --target "$t"
done

lib=libneurone_npps_ffi.a
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
lipo -create \
  "$common/target/aarch64-apple-ios-sim/release/$lib" \
  "$common/target/x86_64-apple-ios/release/$lib" \
  -output "$tmp/$lib"

rm -rf "$out"
mkdir -p "$(dirname "$out")"
xcodebuild -create-xcframework \
  -library "$common/target/aarch64-apple-ios/release/$lib" -headers "$common/npps-ffi/include" \
  -library "$tmp/$lib" -headers "$common/npps-ffi/include" \
  -output "$out"
echo "built $out"
