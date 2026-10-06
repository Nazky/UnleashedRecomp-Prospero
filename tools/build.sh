#!/usr/bin/env bash
# Sonic Unleashed Recompiled - Native PS5 Build Script (PPSA99902)
# Built on ps5-native-app-boilerplate, PS5_Vulkan, PS5_Mesa (RADV), and Plume.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PLUME_ROOT="${PLUME_ROOT:-$ROOT/thirdparty/plume}"

# Locate or bootstrap .deps (supports local .deps or shared /opt/ps5-cache/refs/PS5_Vulkan/.deps)
DEPS_ROOT="$ROOT/.deps"
if [[ ! -d "$DEPS_ROOT/native/ps5-payload-sdk" && -d "/opt/ps5-cache/refs/PS5_Vulkan/.deps/native/ps5-payload-sdk" ]]; then
    DEPS_ROOT="/opt/ps5-cache/refs/PS5_Vulkan/.deps"
fi

SDK_ROOT="${PS5_PAYLOAD_SDK:-$DEPS_ROOT/native/ps5-payload-sdk}"
ZLIB_ROOT="$DEPS_ROOT/native/zlib/root"
if [[ -f "$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a" ]]; then
    RADV_ARCHIVE="${RADV_ARCHIVE:-$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a}"
else
    RADV_ARCHIVE="${RADV_ARCHIVE:-$DEPS_ROOT/native/radv/lib/libvulkan_radeon.ps5.a}"
fi
PACBREW_SYSROOT="${PACBREW_SYSROOT:-$DEPS_ROOT/pacbrew/v0.40.2/sysroot/user/homebrew}"

if [[ ! -x "$SDK_ROOT/bin/prospero-lld" || ! -f "$RADV_ARCHIVE" || ! -d "$PACBREW_SYSROOT" ]]; then
    echo "==> [deps] Bootstrapping PS5 SDK, RADV, and PacBrew dependencies into $ROOT/.deps..."
    bash "$ROOT/tools/setup-native-dependencies.sh"
    bash "$ROOT/tools/build-radv.sh" release
    bash "$ROOT/tools/setup-pacbrew-dependencies.sh" --all >/dev/null
    DEPS_ROOT="$ROOT/.deps"
    SDK_ROOT="$DEPS_ROOT/native/ps5-payload-sdk"
    ZLIB_ROOT="$DEPS_ROOT/native/zlib/root"
    RADV_ARCHIVE="$DEPS_ROOT/native/radv-release/lib/libvulkan_radeon.ps5.a"
    PACBREW_SYSROOT="$DEPS_ROOT/pacbrew/v0.40.2/sysroot/user/homebrew"
fi

TITLE_ID="PPSA99902"
MODULE_SDK=0x02000009
COMPANION_SDK=0x08050001
FSELF_MAGIC=0x1D3D154F

BUILD_DIR="$ROOT/build"
OBJ_DIR="$BUILD_DIR/obj"
STUB_DIR="$BUILD_DIR/stubs"
DIST_DIR="$ROOT/dist"
PKG_DIR="$ROOT/pkg"
NATIVE_DIR="$ROOT/tooling/native"
NATIVE_TOOL="${NATIVE_TOOL:-$BUILD_DIR/host/ps5-native-tool}"
JOBS="${BUILD_JOBS:-$(nproc 2>/dev/null || echo 4)}"

mkdir -p "$BUILD_DIR/host" "$OBJ_DIR" "$STUB_DIR" "$DIST_DIR" "$PKG_DIR"

HOST_CXX="${CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
chmod +x "$ROOT/tools/bin/"* 2>/dev/null || true
if [[ ! -x "$NATIVE_TOOL" ]]; then
    if [[ -x "$ROOT/tools/bin/ps5-native-tool" ]]; then
        cp -f "$ROOT/tools/bin/ps5-native-tool" "$NATIVE_TOOL"
        chmod +x "$NATIVE_TOOL"
    else
        ZLIB_ARCHIVE="$(find "$ZLIB_ROOT" -type f -name libz.a -print -quit)"
        "$HOST_CXX" -std=c++20 -O2 -Wall -Wextra \
            -I "$ZLIB_ROOT/usr/include" \
            "$NATIVE_DIR/native_app_builder.cpp" \
            "$NATIVE_DIR/self_container.cpp" \
            "$NATIVE_DIR/elf_object.cpp" \
            "$NATIVE_DIR/sce_module_writer.cpp" \
            "$ZLIB_ARCHIVE" \
            -o "$NATIVE_TOOL"
    fi
fi

bash "$ROOT/tools/prepare-assets.sh"

# Automatically run recomp-xex.sh if retail files were placed in ./ressources/ (or UnleashedRecompLib/private)
bash "$ROOT/tools/recomp-xex.sh"

# Generate embedded BIN2C resources into $BUILD_DIR/res if not yet generated
bash "$ROOT/tools/generate-resources.sh" "$BUILD_DIR"

cc() { PS5_PAYLOAD_SDK="$SDK_ROOT" sh "$ROOT/tooling/prospero-clang18" "$@"; }

COMMON_INCLUDES=(
    -I"$BUILD_DIR"
    -I"$ROOT/UnleashedRecomp"
    -I"$ROOT/UnleashedRecomp/api"
    -I"$ROOT/UnleashedRecompLib"
    -I"$ROOT/thirdparty/concurrentqueue"
    -I"$ROOT/thirdparty/ddspp"
    -I"$ROOT/thirdparty/imgui"
    -I"$ROOT/thirdparty/implot"
    -I"$ROOT/thirdparty/json/include"
    -I"$ROOT/thirdparty/magic_enum/include"
    -I"$ROOT/thirdparty/o1heap"
    -I"$ROOT/thirdparty/stb"
    -I"$ROOT/tools/XenonRecomp/thirdparty/tomlplusplus/include"
    -I"$ROOT/thirdparty/unordered_dense/include"
    -I"$ROOT/tools/bc_diff"
    -I"$ROOT/tools/XenonRecomp/XenonUtils"
    -I"$ROOT/tools/XenonRecomp/thirdparty/simde"
    -I"$ROOT/tools/XenonRecomp/thirdparty/fmt/include"
    -I"$ROOT/tools/XenonRecomp/thirdparty/xxHash"
    -I"$ROOT/tools/XenonRecomp/thirdparty/libmspack/libmspack/mspack"
    -I"$ROOT/tools/XenonRecomp/thirdparty/tiny-AES-c"
    -I"$ROOT/tools/XenonRecomp/thirdparty/TinySHA1"
    -I"$ROOT/tools/XenonRecomp/thirdparty/disasm"
    -I"$ROOT/tools/XenosRecomp/thirdparty/smol-v/source"
    -I"$PLUME_ROOT"
    -I"$PLUME_ROOT/contrib/Vulkan-Headers/include"
    -I"$PLUME_ROOT/contrib/volk"
    -I"$PLUME_ROOT/contrib/VulkanMemoryAllocator/include"
    -I"$PLUME_ROOT/contrib/SPIRV-Reflect"
    -I"$PACBREW_SYSROOT/include"
    -I"$PACBREW_SYSROOT/include/SDL2"
)

COMMON_DEFINES=(
    -D__PROSPERO__
    -DVK_NO_PROTOTYPES
    -DSDL_MAIN_HANDLED
    -DNOMINMAX
    -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    -D_CRT_SECURE_NO_WARNINGS
    -DNDEBUG
)

COMMON_WARN_FLAGS=(
    -fno-strict-aliasing
    -ffp-model=strict
    -Wno-unknown-warning-option
    -Wno-nontrivial-memcall
    -Wno-missing-template-arg-list-after-template-kw
    -Wno-enum-constexpr-conversion
    -Wno-switch
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-void-pointer-to-int-cast
    -Wno-int-to-void-pointer-cast
    -Wno-invalid-offsetof
    -Wno-null-arithmetic
    -Wno-null-conversion
    -Wno-tautological-undefined-compare
    -Wno-deprecated-declarations
    -Wno-format
)

CFLAGS=(-std=c11 -O2 -ffunction-sections -fdata-sections "${COMMON_DEFINES[@]}" "${COMMON_INCLUDES[@]}")
CXXFLAGS=(-std=c++20 -O2 -fexceptions -funwind-tables -ffunction-sections -fdata-sections "${COMMON_DEFINES[@]}" "${COMMON_WARN_FLAGS[@]}" "${COMMON_INCLUDES[@]}")

obj_path_for() {
    local src="$1"
    local rel="${src#$ROOT/}"
    rel="${rel//\//__}"
    printf '%s/%s.o' "$OBJ_DIR" "$rel"
}

OBJECTS=()
QUEUE_C=()
QUEUE_CXX_NOPCH=()
QUEUE_CXX_PPCPCH=()
QUEUE_CXX_STDPCH=()

queue_c() {
    local src="$1"
    local obj
    obj="$(obj_path_for "$src")"
    OBJECTS+=("$obj")
    if [[ ! -f "$obj" || "$src" -nt "$obj" ]]; then
        QUEUE_C+=("$src")
    fi
}

queue_cxx() {
    local src="$1"
    local pch_mode="${2:-none}"
    local obj
    obj="$(obj_path_for "$src")"
    OBJECTS+=("$obj")
    if [[ ! -f "$obj" || "$src" -nt "$obj" ]]; then
        case "$pch_mode" in
            ppc) QUEUE_CXX_PPCPCH+=("$src") ;;
            std) QUEUE_CXX_STDPCH+=("$src") ;;
            *)   QUEUE_CXX_NOPCH+=("$src") ;;
        esac
    fi
}

run_parallel_compile() {
    local active=0
    for cmd_idx in "$@"; do
        eval "$cmd_idx" &
        active=$((active + 1))
        if (( active >= JOBS )); then
            wait -n
            active=$((active - 1))
        fi
    done
    wait
}

echo "==> [1/7] Compiling PS5 CRT & AGC stubs..."
cc -std=c++20 -O2 -Wall -Wextra -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
    -c "$NATIVE_DIR/app_crt.cpp" -o "$OBJ_DIR/app_crt.o"
OBJECTS+=("$OBJ_DIR/app_crt.o")

stub() {
    local library=$1 source=$2
    cc -std=c11 -O2 -fPIC -c "$ROOT/$source" -o "$OBJ_DIR/${library}_stub.o"
    "$SDK_ROOT/bin/prospero-lld" --shared -soname "${library}.prx" \
        -o "$STUB_DIR/${library}.so" "$OBJ_DIR/${library}_stub.o"
}
stub libSceAgc vendor/ps5/sdk/stubs/agc_canary_link_stub.c
stub libSceAgcDriver vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c

echo "==> [2/7] Queueing Plume, XenonUtils & thirdparty sources..."
queue_cxx "$PLUME_ROOT/plume_vulkan.cpp" none
queue_c "$ROOT/thirdparty/o1heap/o1heap.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/xxHash/xxhash.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/disasm/disasm.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/disasm/ppc-dis.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/tiny-AES-c/aes.c"
queue_c "$ROOT/tools/XenonRecomp/thirdparty/libmspack/libmspack/mspack/lzxd.c"
queue_cxx "$ROOT/tools/XenonRecomp/thirdparty/fmt/src/format.cc" none
queue_cxx "$ROOT/tools/XenonRecomp/thirdparty/fmt/src/os.cc" none

for src in "$ROOT"/tools/XenonRecomp/XenonUtils/*.cpp; do
    queue_cxx "$src" none
done

for src in \
    "$ROOT/thirdparty/imgui/backends/imgui_impl_sdl2.cpp" \
    "$ROOT/thirdparty/imgui/imgui.cpp" \
    "$ROOT/thirdparty/imgui/imgui_demo.cpp" \
    "$ROOT/thirdparty/imgui/imgui_draw.cpp" \
    "$ROOT/thirdparty/imgui/imgui_tables.cpp" \
    "$ROOT/thirdparty/imgui/imgui_widgets.cpp" \
    "$ROOT/thirdparty/implot/implot.cpp" \
    "$ROOT/thirdparty/implot/implot_items.cpp" \
    "$ROOT/tools/XenosRecomp/thirdparty/smol-v/source/smolv.cpp"; do
    [[ -f "$src" ]] && queue_cxx "$src" none
done

echo "==> [3/7] Queueing UnleashedRecompLib (PPC + Xenos Shader Cache)..."
for src in "$ROOT"/UnleashedRecompLib/ppc/*.cpp; do
    queue_cxx "$src" ppc
done
for src in "$ROOT"/UnleashedRecompLib/shader/*.cpp; do
    queue_cxx "$src" ppc
done

echo "==> [4/7] Queueing UnleashedRecomp embedded BIN2C resources..."
while IFS= read -r -d '' cfile; do
    queue_c "$cfile"
done < <(find "$BUILD_DIR/res" -name "*.c" -print0 | sort -z)

echo "==> [5/7] Queueing UnleashedRecomp C++ sources..."
UNLEASHED_SRCS=(
    "$ROOT/UnleashedRecomp/app.cpp"
    "$ROOT/UnleashedRecomp/exports.cpp"
    "$ROOT/UnleashedRecomp/main.cpp"
    "$ROOT/UnleashedRecomp/misc_impl.cpp"
    "$ROOT/UnleashedRecomp/preload_executable.cpp"
    "$ROOT/UnleashedRecomp/sdl_listener.cpp"
    "$ROOT/UnleashedRecomp/stdafx.cpp"
    "$ROOT/UnleashedRecomp/version.cpp"
    "$ROOT"/UnleashedRecomp/kernel/*.cpp
    "$ROOT"/UnleashedRecomp/kernel/io/*.cpp
    "$ROOT"/UnleashedRecomp/locale/*.cpp
    "$ROOT"/UnleashedRecomp/os/linux/*.cpp
    "$ROOT"/UnleashedRecomp/cpu/*.cpp
    "$ROOT"/UnleashedRecomp/gpu/*.cpp
    "$ROOT"/UnleashedRecomp/gpu/imgui/*.cpp
    "$ROOT"/UnleashedRecomp/apu/*.cpp
    "$ROOT"/UnleashedRecomp/apu/driver/*.cpp
    "$ROOT"/UnleashedRecomp/hid/*.cpp
    "$ROOT"/UnleashedRecomp/hid/driver/*.cpp
    "$ROOT"/UnleashedRecomp/patches/*.cpp
    "$ROOT"/UnleashedRecomp/ui/*.cpp
    "$ROOT"/UnleashedRecomp/install/*.cpp
    "$ROOT"/UnleashedRecomp/install/hashes/*.cpp
    "$ROOT"/UnleashedRecomp/user/*.cpp
    "$ROOT"/UnleashedRecomp/mod/*.cpp
)
for src in "${UNLEASHED_SRCS[@]}"; do
    queue_cxx "$src" std
done

echo "==> [6/7] Compiling queued translation units in parallel (-j $JOBS)..."
if (( ${#QUEUE_C[@]} > 0 )); then
    CMDS=()
    for src in "${QUEUE_C[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" ]] && continue
        CMDS+=("cc \"\${CFLAGS[@]}\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

if (( ${#QUEUE_CXX_NOPCH[@]} > 0 )); then
    CMDS=()
    for src in "${QUEUE_CXX_NOPCH[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" ]] && continue
        CMDS+=("cc \"\${CXXFLAGS[@]}\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

if (( ${#QUEUE_CXX_PPCPCH[@]} > 0 )); then
    PPC_PCH="$BUILD_DIR/ppc_recomp_shared.h.pch"
    if [[ "${CLEAN_BUILD:-0}" == "1" || ! -f "$PPC_PCH" || "$ROOT/UnleashedRecompLib/ppc/ppc_recomp_shared.h" -nt "$PPC_PCH" ]]; then
        cc "${CXXFLAGS[@]}" -x c++-header "$ROOT/UnleashedRecompLib/ppc/ppc_recomp_shared.h" -o "$PPC_PCH"
    fi
    CMDS=()
    for src in "${QUEUE_CXX_PPCPCH[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" && "$obj" -nt "$PPC_PCH" ]] && continue
        CMDS+=("cc \"\${CXXFLAGS[@]}\" -include-pch \"$PPC_PCH\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

if (( ${#QUEUE_CXX_STDPCH[@]} > 0 )); then
    STD_PCH="$BUILD_DIR/stdafx.h.pch"
    if [[ "${CLEAN_BUILD:-0}" == "1" || ! -f "$STD_PCH" || "$ROOT/UnleashedRecomp/stdafx.h" -nt "$STD_PCH" ]]; then
        cc "${CXXFLAGS[@]}" -x c++-header "$ROOT/UnleashedRecomp/stdafx.h" -o "$STD_PCH"
    fi
    CMDS=()
    for src in "${QUEUE_CXX_STDPCH[@]}"; do
        obj="$(obj_path_for "$src")"
        [[ "${CLEAN_BUILD:-0}" != "1" && -f "$obj" && "$obj" -nt "$src" && "$obj" -nt "$STD_PCH" ]] && continue
        CMDS+=("cc \"\${CXXFLAGS[@]}\" -include-pch \"$STD_PCH\" -c \"$src\" -o \"$obj\"")
    done
    (( ${#CMDS[@]} > 0 )) && run_parallel_compile "${CMDS[@]}"
fi

echo "==> [7/7] Linking PS5 PIE ELF with RADV (libvulkan_radeon.ps5.a) + PacBrew (SDL2, SDL2_mixer, vorbis, ogg, zstd)..."
source "$ROOT/tools/radv-link.sh"
radv_link_recipe "$ROOT" "$SDK_ROOT" "$RADV_ARCHIVE" || exit 2

"$SDK_ROOT/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" \
    --version-script "$NATIVE_DIR/app-symbols.map" --exclude-libs=ALL \
    -e _start -o "$BUILD_DIR/llvm-pie.elf" \
    "${OBJECTS[@]}" \
    --start-group \
    "$PACBREW_SYSROOT/lib/libSDL2_mixer.a" \
    "$PACBREW_SYSROOT/lib/libvorbisfile.a" \
    "$PACBREW_SYSROOT/lib/libvorbis.a" \
    "$PACBREW_SYSROOT/lib/libogg.a" \
    "$PACBREW_SYSROOT/lib/libSDL2.a" \
    "$PACBREW_SYSROOT/lib/libiconv.a" \
    "$PACBREW_SYSROOT/lib/libzstd.a" \
    --end-group \
    "$STUB_DIR/libSceAgc.so" "$STUB_DIR/libSceAgcDriver.so" \
    "${radv_link_inputs[@]}" \
    --as-needed "$SDK_ROOT"/target/lib/*.so

echo "==> [package] Converting and signing PS5 eboot.bin ($TITLE_ID)..."
"$NATIVE_TOOL" link --in "$BUILD_DIR/llvm-pie.elf" --out "$BUILD_DIR/eboot.elf" \
    --stub-dir "$SDK_ROOT/target/lib" --stub "$STUB_DIR/libSceAgc.so" \
    --stub "$STUB_DIR/libSceAgcDriver.so" --module-sdk "$MODULE_SDK" \
    --companion-sdk "$COMPANION_SDK" --file-name eboot.elf

APP_DIR="$DIST_DIR/$TITLE_ID"
rm -rf -- "$APP_DIR"
mkdir -p "$APP_DIR/sce_sys" "$APP_DIR/sce_module"
"$NATIVE_TOOL" self --sign --in "$BUILD_DIR/eboot.elf" --out "$APP_DIR/eboot.bin" --magic "$FSELF_MAGIC"
cp "$ROOT/sce_sys/param.json" "$APP_DIR/sce_sys/param.json"
for asset in icon0.png pic0.dds pic1.dds snd0.at9; do
    if [[ -f "$ROOT/sce_sys/$asset" ]]; then
        cp -lf "$ROOT/sce_sys/$asset" "$APP_DIR/sce_sys/$asset" 2>/dev/null || cp "$ROOT/sce_sys/$asset" "$APP_DIR/sce_sys/$asset"
    fi
done
[[ -f "$ROOT/runtime/libc.prx" ]] || bash "$ROOT/tools/rebuild-libc.sh"
cp "$ROOT/runtime/libc.prx" "$APP_DIR/sce_module/libc.prx"

# If ./ressources/game exists with retail files, stage ressources/ into the PS5 title folder
if [[ -d "$ROOT/ressources/game" ]] && [[ -n "$(ls -A "$ROOT/ressources/game" 2>/dev/null)" ]]; then
    echo "==> [package] Staging ./ressources/ into $APP_DIR/ressources/..."
    mkdir -p "$APP_DIR/ressources"
    cp -al "$ROOT/ressources/." "$APP_DIR/ressources/" 2>/dev/null || cp -a "$ROOT/ressources/." "$APP_DIR/ressources/"
fi

"$NATIVE_TOOL" self --inspect --file "$APP_DIR/sce_module/libc.prx"
"$NATIVE_TOOL" self --inspect --file "$APP_DIR/eboot.bin"

rm -rf -- "$PKG_DIR/$TITLE_ID"
mkdir -p "$PKG_DIR"
cp -al "$APP_DIR" "$PKG_DIR/$TITLE_ID" 2>/dev/null || cp -a "$APP_DIR" "$PKG_DIR/$TITLE_ID"

printf 'Build complete:\n  dist: %s (%s bytes)\n  pkg:  %s\n' \
    "$APP_DIR" "$(stat -c %s "$APP_DIR/eboot.bin")" "$PKG_DIR/$TITLE_ID"
