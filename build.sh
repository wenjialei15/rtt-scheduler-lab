#!/usr/bin/env bash
#
# Builds the lab with zig as the C compiler. Tested with zig 0.17.
#   ./build.sh
#   CC="python -m ziglang" ./build.sh
#   RTT=/path/to/rt-thread ./build.sh

set -euo pipefail

cd "$(dirname "$0")"

RTT="${RTT:-third_party/rt-thread}"
CC="${CC:-zig cc}"

if [ ! -f "$RTT/src/thread.c" ]; then
    echo "missing $RTT — run:" >&2
    echo "  git submodule update --init third_party/rt-thread" >&2
    exit 1
fi

mkdir -p build

# 32-bit on purpose: RT-Thread's rt_base_t is `long`, which is 32-bit on
# Windows x64. A 64-bit build truncates pointers and crashes at startup.
# shellcheck disable=SC2086
$CC -target x86-windows-gnu -O1 -g \
    -D_Win32 -fno-builtin -w \
    -Iport \
    -I"$RTT/include" \
    -Iport/shim \
    "$RTT"/src/*.c \
    "$RTT/libcpu/sim/win32/cpu_port.c" \
    port/board.c \
    src/main.c \
    -lwinmm \
    -o build/rtt.exe

echo "built build/rtt.exe"
