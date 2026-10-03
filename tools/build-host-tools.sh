#!/usr/bin/env bash
# UnleashedRecomp - Build host recompiler tools (XenonRecomp, XenosRecomp, x_decompress, file_to_c)
# and fetch renderbag/dxc-bin (libdxcompiler.so) if not already present.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS_BIN="$ROOT/tools/bin"
XENON_DIR="$ROOT/tools/XenonRecomp"
XENOS_DIR="$ROOT/tools/XenosRecomp"
DXC_DIR="${DXC_ROOT:-}"

if [[ -z "$DXC_DIR" ]]; then
    if [[ -f "$XENOS_DIR/thirdparty/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
        DXC_DIR="$XENOS_DIR/thirdparty/dxc-bin"
    elif [[ -f "$ROOT/.deps/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
        DXC_DIR="$ROOT/.deps/dxc-bin"
    elif [[ -f "/opt/ps5-cache/refs/dxc-bin/lib/x64/libdxcompiler.so" ]]; then
        DXC_DIR="/opt/ps5-cache/refs/dxc-bin"
    else
        DXC_DIR="$ROOT/.deps/dxc-bin"
        echo "==> [host-tools] Cloning https://github.com/renderbag/dxc-bin.git into $DXC_DIR..." >&2
        mkdir -p "$(dirname "$DXC_DIR")"
        git clone --depth 1 https://github.com/renderbag/dxc-bin.git "$DXC_DIR" >&2
    fi
fi

mkdir -p "$TOOLS_BIN"
chmod +x "$TOOLS_BIN/"* 2>/dev/null || true
CXX="${HOST_CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
CC="${HOST_CC:-$(command -v gcc || command -v clang-18 || command -v clang)}"

if [[ ! -x "$TOOLS_BIN/x_decompress" ]]; then
    echo "==> [host-tools] Building x_decompress..." >&2
    BUILD_HOST="$ROOT/build/host-xdec"
    mkdir -p "$BUILD_HOST"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" \
        -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/lzxd.c" -o "$BUILD_HOST/lzxd.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" \
        -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/system.c" -o "$BUILD_HOST/system.o"
    "$CXX" -std=c++20 -O2 \
        -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" \
        "$ROOT/tools/x_decompress/x_decompress.cpp" \
        "$BUILD_HOST/lzxd.o" "$BUILD_HOST/system.o" \
        -o "$TOOLS_BIN/x_decompress"
    rm -rf "$BUILD_HOST"
fi

if [[ ! -x "$TOOLS_BIN/file_to_c" ]]; then
    echo "==> [host-tools] Building file_to_c..." >&2
    "$CXX" -std=c++20 -O2 \
        "$ROOT/tools/file_to_c/file_to_c.cpp" \
        -lzstd \
        -o "$TOOLS_BIN/file_to_c"
fi

if [[ ! -x "$TOOLS_BIN/png_to_bc7_dds" || "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" -nt "$TOOLS_BIN/png_to_bc7_dds" ]]; then
    echo "==> [host-tools] Building png_to_bc7_dds..." >&2
    "$CXX" -std=c++20 -O3 -fopenmp \
        -I"$ROOT/thirdparty/stb" \
        "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" \
        -o "$TOOLS_BIN/png_to_bc7_dds" || \
    "$CXX" -std=c++20 -O3 \
        -I"$ROOT/thirdparty/stb" \
        "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" \
        -o "$TOOLS_BIN/png_to_bc7_dds"
fi

if [[ ! -x "$TOOLS_BIN/wav_to_at9" || "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" -nt "$TOOLS_BIN/wav_to_at9" ]]; then
    echo "==> [host-tools] Building wav_to_at9 (native ATRAC9 encoder)..." >&2
    BUILD_AT9="$ROOT/build/host-at9"
    mkdir -p "$BUILD_AT9"
    for csrc in "$ROOT"/thirdparty/libatrac9/src/*.c; do
        "$CC" -O3 -c "$csrc" -o "$BUILD_AT9/$(basename "${csrc%.c}").c.o"
    done
    "$CXX" -std=c++20 -O3 \
        -I"$ROOT/thirdparty/stb" \
        -I"$ROOT/thirdparty/libatrac9/src" \
        "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" \
        "$BUILD_AT9"/*.o -lm \
        -o "$TOOLS_BIN/wav_to_at9"
    rm -rf "$BUILD_AT9"
fi

if [[ ! -x "$TOOLS_BIN/XenonRecomp" ]]; then
    echo "==> [host-tools] Building XenonRecomp..." >&2
    BUILD_HOST="$ROOT/build/host-xenon"
    mkdir -p "$BUILD_HOST"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/disasm" -c "$XENON_DIR/thirdparty/disasm/disasm.c" -o "$BUILD_HOST/disasm.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/disasm" -c "$XENON_DIR/thirdparty/disasm/ppc-dis.c" -o "$BUILD_HOST/ppc-dis.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/tiny-AES-c" -c "$XENON_DIR/thirdparty/tiny-AES-c/aes.c" -o "$BUILD_HOST/aes.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/lzxd.c" -o "$BUILD_HOST/lzxd.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack" -c "$XENON_DIR/thirdparty/libmspack/libmspack/mspack/system.c" -o "$BUILD_HOST/system.o"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/xxHash" -c "$XENON_DIR/thirdparty/xxHash/xxhash.c" -o "$BUILD_HOST/xxhash.o"
    XENON_CXXFLAGS=(
        -std=c++20 -O2
        -Wno-switch -Wno-unused-variable -Wno-pointer-arith -fms-extensions
        -DXENON_RECOMP_USE_ALIAS -D_CRT_SECURE_NO_WARNINGS
        -include "$XENON_DIR/XenonRecomp/pch.h"
        -I"$XENON_DIR/XenonRecomp"
        -I"$XENON_DIR/XenonAnalyse"
        -I"$XENON_DIR/XenonUtils"
        -I"$XENON_DIR/thirdparty/disasm"
        -I"$XENON_DIR/thirdparty/fmt/include"
        -I"$XENON_DIR/thirdparty/tomlplusplus/include"
        -I"$XENON_DIR/thirdparty/xxHash"
        -I"$XENON_DIR/thirdparty/libmspack/libmspack/mspack"
        -I"$XENON_DIR/thirdparty/tiny-AES-c"
        -I"$XENON_DIR/thirdparty/TinySHA1"
        -I"$XENON_DIR/thirdparty/simde"
    )
    idx=0
    for cpp_src in \
        "$XENON_DIR"/XenonRecomp/*.cpp \
        "$XENON_DIR/XenonAnalyse/function.cpp" \
        "$XENON_DIR"/XenonUtils/*.cpp \
        "$XENON_DIR/thirdparty/fmt/src/format.cc" \
        "$XENON_DIR/thirdparty/fmt/src/os.cc"
    do
        idx=$((idx + 1))
        TMPDIR="$BUILD_HOST" "$CXX" "${XENON_CXXFLAGS[@]}" -c "$cpp_src" -o "$BUILD_HOST/cpp_${idx}.o"
    done
    TMPDIR="$BUILD_HOST" "$CXX" -std=c++20 -O2 "$BUILD_HOST"/*.o -lpthread -o "$TOOLS_BIN/XenonRecomp"
    rm -rf "$BUILD_HOST"
fi

if [[ ! -x "$TOOLS_BIN/XenosRecomp" ]]; then
    echo "==> [host-tools] Building XenosRecomp (with -DUNLEASHED_RECOMP)..." >&2
    BUILD_HOST="$ROOT/build/host-xenos"
    mkdir -p "$BUILD_HOST"
    "$CC" -O2 -I"$XENON_DIR/thirdparty/xxHash" -c "$XENON_DIR/thirdparty/xxHash/xxhash.c" -o "$BUILD_HOST/xxhash.o"
    XENOS_CXXFLAGS=(
        -std=c++20 -O2
        -Wno-switch -Wno-unused-variable -Wno-pointer-arith -fms-extensions
        -DUNLEASHED_RECOMP
        -include "$XENOS_DIR/XenosRecomp/pch.h"
        -I"$XENOS_DIR/XenosRecomp"
        -I"$XENOS_DIR/thirdparty/smol-v/source"
        -I"$XENOS_DIR/thirdparty/dxc-bin/inc"
        -I"$DXC_DIR/inc"
        -I"$XENON_DIR/thirdparty/fmt/include"
        -I"$XENON_DIR/thirdparty/xxHash"
    )
    idx=0
    for cpp_src in \
        "$XENOS_DIR/XenosRecomp/main.cpp" \
        "$XENOS_DIR/XenosRecomp/dxc_compiler.cpp" \
        "$XENOS_DIR/XenosRecomp/shader_recompiler.cpp" \
        "$XENOS_DIR/thirdparty/smol-v/source/smolv.cpp" \
        "$XENON_DIR/thirdparty/fmt/src/format.cc" \
        "$XENON_DIR/thirdparty/fmt/src/os.cc"
    do
        idx=$((idx + 1))
        TMPDIR="$BUILD_HOST" "$CXX" "${XENOS_CXXFLAGS[@]}" -c "$cpp_src" -o "$BUILD_HOST/cpp_${idx}.o"
    done
    TMPDIR="$BUILD_HOST" "$CXX" -std=c++20 -O2 \
        "$BUILD_HOST"/*.o \
        -L"$DXC_DIR/lib/x64" \
        -Wl,-rpath,'$ORIGIN/../XenosRecomp/thirdparty/dxc-bin/lib/x64:$ORIGIN/../../.deps/dxc-bin/lib/x64' \
        -ldxcompiler -lzstd -lpthread \
        -o "$TOOLS_BIN/XenosRecomp"
    rm -rf "$BUILD_HOST"
fi

echo "DXC_LIB_DIR=$DXC_DIR/lib/x64"
