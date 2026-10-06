#!/usr/bin/env bash
# Builds the C ABI as a static library and runs smoke.c against neurone_npps.h. Run from anywhere:
#   common/npps-ffi/tests/c/run.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
common="$(cd "$here/../../.." && pwd)"
cargo build --release --locked --manifest-path "$common/Cargo.toml" -p neurone-npps-ffi
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -I "$common/npps-ffi/include" "$here/smoke.c" \
   "$common/target/release/libneurone_npps_ffi.a" -lpthread -ldl -lm -o "$out/smoke"
"$out/smoke"
