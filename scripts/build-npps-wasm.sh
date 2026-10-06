#!/usr/bin/env bash
# Builds the shared NPPS core's C ABI (common/npps-ffi, OI-NPPS-CORE-01) as WebAssembly for the web
# app: app/web/src/generated/neurone_npps.wasm, a git-ignored build output that
# app/web/src/lib/nppsCore.ts loads.
#
#   scripts/build-npps-wasm.sh
#
# Needs a Rust toolchain; the wasm32-unknown-unknown target is added with rustup if it is present.
# Up to date: nothing the library is built from is newer than the output.
set -euo pipefail

root="$(git rev-parse --show-toplevel)"
common="$root/common"
out="$root/common/generated/neurone_npps.wasm"
sim="$root/simulator/js/generated/neurone_npps.wasm"
export PATH="$HOME/.cargo/bin:$PATH"

if ! command -v cargo >/dev/null 2>&1; then
  echo "error: cargo not found. The web app compiles protocols with the Rust NPPS core; install Rust (https://rustup.rs)." >&2
  exit 1
fi

if [ -f "$out" ] && [ -z "$(find "$common/npps-core" "$common/npps-ffi" "$common/npps/fields.json" \
      "$common/Cargo.toml" "$common/Cargo.lock" "$root/scripts/build-npps-wasm.sh" \
      -type f -not -path '*/target/*' -newer "$out" 2>/dev/null | head -n 1)" ]; then
  echo "neurone_npps.wasm is up to date"
  exit 0
fi

if command -v rustup >/dev/null 2>&1; then
  rustup target add wasm32-unknown-unknown
fi
cargo build --release --locked --manifest-path "$common/Cargo.toml" -p neurone-npps-ffi --target wasm32-unknown-unknown
mkdir -p "$(dirname "$out")"
cp "$common/target/wasm32-unknown-unknown/release/neurone_npps_ffi.wasm" "$out"
mkdir -p "$(dirname "$sim")"
cp "$out" "$sim"
echo "built $out ($(wc -c <"$out") bytes)"
