#!/usr/bin/env bash
# Builds and runs the host-side wear-leveling integrity test against the real
# quantum/wear_leveling/wear_leveling.c.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

cc -std=c11 -Wall -Wextra -Wno-unused-parameter -O1 \
    -DBACKING_STORE_WRITE_SIZE=2 \
    -DWEAR_LEVELING_BACKING_SIZE="(128*1024)" \
    -DWEAR_LEVELING_LOGICAL_SIZE="(64*1024)" \
    -I"$root/quantum" \
    -I"$root/quantum/wear_leveling" \
    -I"$root/lib/fnv" \
    -o "$out/test_integrity" \
    "$root/util/wear_leveling_test/test_integrity.c" \
    "$root/quantum/wear_leveling/wear_leveling.c" \
    "$root/lib/fnv/hash_64a.c"

"$out/test_integrity"
