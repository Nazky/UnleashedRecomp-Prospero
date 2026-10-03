#!/usr/bin/env bash
# PS5 Vulkan - build RADV from mihawk-99/PS5_Mesa at its pinned revision.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mesa_revision=0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8
mesa_repo_url="https://github.com/mihawk-99/PS5_Mesa.git"
variant=${1:-release}
sdk="$root/.deps/native/ps5-payload-sdk"
source_tree="$root/.deps/work/radv-src"
case $variant in
    debug)
        build="$root/.deps/work/radv-build-ps5"
        install="$root/.deps/native/radv"
        ndebug=false
        ;;
    release)
        build="$root/.deps/work/radv-build-ps5-release"
        install="$root/.deps/native/radv-release"
        ndebug=true
        ;;
    *) echo "usage: tools/build-radv.sh [debug|release]" >&2; exit 2 ;;
esac
meson=${MESON:-$(command -v meson || echo "$HOME/.local/bin/meson")}
ninja=${NINJA:-$(command -v ninja || command -v ninja-build || echo "$HOME/.local/bin/ninja")}

[[ -x $meson && -x $ninja ]] || {
    echo "ERROR: meson and ninja are needed." >&2
    echo "  Fedora: sudo dnf install meson ninja-build glslang bison flex python3-mako python3-ply python3-pyyaml" >&2
    echo "  Ubuntu: sudo apt-get install meson ninja-build glslang-tools bison flex python3-mako python3-ply python3-yaml" >&2
    exit 2
}
command -v rsync > /dev/null || { echo "ERROR: rsync is needed (sudo dnf install rsync / sudo apt-get install rsync)" >&2; exit 2; }
command -v glslangValidator > /dev/null || {
    echo "ERROR: glslangValidator is needed to compile RADV BVH shaders." >&2
    echo "  Fedora: sudo dnf install glslang" >&2
    echo "  Ubuntu: sudo apt-get install glslang-tools" >&2
    exit 2
}
[[ -f $sdk/.ps5-sdk-revision ]] || { echo "run tools/setup-native-dependencies.sh first" >&2; exit 2; }
if [[ -f $install/PROVENANCE.txt ]] && grep -q "^revision: $mesa_revision$" "$install/PROVENANCE.txt" &&
    grep -q "^sdk: $(cat "$sdk/.ps5-sdk-revision")$" "$install/PROVENANCE.txt"; then
    echo "==> [radv] $install is RADV ($variant) at ${mesa_revision:0:12}"
    exit 0
fi

# Ensure Python mako, ply, and yaml modules are available for Mesa codegen
if ! python3 -c "import mako, ply, yaml" 2>/dev/null; then
    py_deps="$root/.deps/python-packages"
    mkdir -p "$py_deps"
    export PYTHONPATH="$py_deps:${PYTHONPATH:-}"
    if ! python3 -c "import mako, ply, yaml" 2>/dev/null; then
        echo "==> [radv] Installing Python build modules (mako, ply, pyyaml) into $py_deps..." >&2
        python3 -m pip install --quiet --no-warn-conflicts --target "$py_deps" mako ply pyyaml || {
            echo "ERROR: Missing Python modules mako, ply, pyyaml." >&2
            echo "  Fedora: sudo dnf install python3-mako python3-ply python3-pyyaml" >&2
            echo "  Ubuntu: sudo apt-get install python3-mako python3-ply python3-yaml" >&2
            exit 2
        }
    fi
fi

mesa_fork="${PS5_MESA_FORK:-}"
if [[ -z "$mesa_fork" ]]; then
    if [[ -d "$root/../PS5_Mesa/.git" ]]; then
        mesa_fork="$root/../PS5_Mesa"
    elif [[ -d "/opt/ps5-cache/refs/PS5_Mesa/.git" ]]; then
        mesa_fork="/opt/ps5-cache/refs/PS5_Mesa"
    else
        mesa_fork="$root/.deps/src/PS5_Mesa"
    fi
fi
if [[ ! -d "$mesa_fork/.git" ]]; then
    echo "==> [radv] Cloning $mesa_repo_url into $mesa_fork" >&2
    mkdir -p "$(dirname "$mesa_fork")"
    git clone --depth 1 "$mesa_repo_url" "$mesa_fork" >&2
fi
git -C "$mesa_fork" cat-file -e "$mesa_revision^{commit}" 2>/dev/null || {
    git -C "$mesa_fork" fetch origin "$mesa_revision" >&2 || true
}

if [[ ! -f $source_tree/.revision || $(<"$source_tree/.revision") != "$mesa_revision" ]]; then
    staging="$source_tree.new"
    rm -rf "$staging"
    mkdir -p "$staging" "$source_tree"
    git -C "$mesa_fork" archive "$mesa_revision" | tar -x -C "$staging"
    rsync -rlp --checksum --delete --exclude=/.revision --exclude=/subprojects/packagecache/ \
        --exclude=/subprojects/zlib-*/ "$staging/" "$source_tree/"
    rm -rf "$staging"
    printf '%s\n' "$mesa_revision" > "$source_tree/.revision"
fi

clc_bin="$root/.deps/work/radv-clc-bin"
clc_cache_archive="$root/tooling/radv/clc-cache.tar.xz"
clc_cache_dir="$root/.deps/work/radv-clc-cache"

if [[ -f "$clc_cache_archive" && "${FORCE_BUILD_MESA_CLC:-0}" != "1" ]]; then
    echo "==> [radv] Using pre-compiled OpenCL/SPIR-V shaders from tooling/radv/clc-cache.tar.xz (skipping host LLVMSPIRVLib/mesa_clc build)"
    mkdir -p "$clc_cache_dir" "$clc_bin"
    tar -xJf "$clc_cache_archive" -C "$clc_cache_dir"

    cat > "$clc_bin/mesa_clc" <<EOF
#!/usr/bin/env bash
set -euo pipefail
out=""
depfile=""
while [[ \$# -gt 0 ]]; do
    case "\$1" in
        -o) out="\$2"; shift 2 ;;
        --depfile) depfile="\$2"; shift 2 ;;
        --) break ;;
        *) shift ;;
    esac
done
base="\$(basename "\$out")"
cp -f "$clc_cache_dir/\$base" "\$out"
if [[ -n "\$depfile" ]]; then
    printf '%s:\n' "\$out" > "\$depfile"
fi
EOF

    cat > "$clc_bin/vtn_bindgen2" <<EOF
#!/usr/bin/env bash
set -euo pipefail
spv="\$1"
out_cpp="\$2"
out_h="\$3"
cp -f "$clc_cache_dir/\$(basename "\$out_cpp")" "\$out_cpp"
cp -f "$clc_cache_dir/\$(basename "\$out_h")" "\$out_h"
EOF

    chmod +x "$clc_bin/mesa_clc" "$clc_bin/vtn_bindgen2"
else
    clc_build="$root/.deps/work/radv-clc-build"
    if [[ ! -f $clc_build/build.ninja ]]; then
        "$meson" setup "$clc_build" "$source_tree" -Dbuildtype=release -Dmesa-clc=enabled -Dinstall-mesa-clc=true \
            -Dgallium-drivers= -Dvulkan-drivers= -Dplatforms= -Dglx=disabled -Degl=disabled -Dgbm=disabled \
            -Dopengl=false -Dgles1=disabled -Dgles2=disabled -Dllvm=enabled -Dshared-llvm=enabled \
            -Dbuild-tests=false -Dvalgrind=disabled -Dlibunwind=disabled -Dzstd=disabled -Dxmlconfig=disabled \
            -Dtools= > "$clc_build.setup.log" 2>&1 ||
            { tail -20 "$clc_build.setup.log" >&2; exit 1; }
    fi
    "$ninja" -C "$clc_build" src/compiler/clc/mesa_clc src/compiler/spirv/vtn_bindgen2 > "$clc_build.log" 2>&1 ||
        { grep -E "error|FAILED" "$clc_build.log" | head -20 >&2; exit 1; }
    mkdir -p "$clc_bin"
    ln -sf "$clc_build/src/compiler/clc/mesa_clc" "$clc_bin/mesa_clc"
    ln -sf "$clc_build/src/compiler/spirv/vtn_bindgen2" "$clc_bin/vtn_bindgen2"
fi
export PATH="$clc_bin:$PATH"

constants="$root/.deps/work/radv-cross-constants.ini"
printf "[constants]\nsdk = '%s'\n" "$sdk" > "$constants"
options=(-Dvulkan-drivers=amd -Dgallium-drivers= -Dplatforms= -Dradv-winsys=ps5
    -Dllvm=disabled -Damd-use-llvm=false -Dvideo-codecs=
    -Dbuildtype=debugoptimized -Db_ndebug="$ndebug"
    -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled
    -Dvalgrind=disabled -Dlibunwind=disabled -Dzstd=disabled -Dzlib=enabled --force-fallback-for=zlib
    -Dexpat=disabled -Dxmlconfig=disabled -Dshader-cache=enabled -Dbuild-tests=false -Dvulkan-layers= -Dtools=
    -Dmesa-clc=system)
if [[ ! -f $build/build.ninja || $(cat "$build/.radv-options" 2>/dev/null) != "${options[*]}" ]]; then
    wipe=()
    [[ -f $build/build.ninja ]] && wipe=(--wipe)
    echo "==> [radv] Configuring PS5 RADV ($variant) with Meson..."
    "$meson" setup "${wipe[@]}" "$build" "$source_tree" \
        --cross-file "$constants" --cross-file "$root/tooling/radv/ps5-cross.ini" \
        "${options[@]}" -Dradv-build-id="$mesa_revision" > "$build.setup.log" 2>&1 ||
        { tail -20 "$build.setup.log" >&2; exit 1; }
elif [[ $(cat "$build/.radv-build-id" 2>/dev/null) != "$mesa_revision" ]]; then
    "$meson" configure "$build" -Dradv-build-id="$mesa_revision" > "$build.setup.log" 2>&1 ||
        { tail -20 "$build.setup.log" >&2; exit 1; }
fi
printf '%s\n' "${options[*]}" > "$build/.radv-options"
printf '%s\n' "$mesa_revision" > "$build/.radv-build-id"
echo "==> [radv] Compiling libvulkan_radeon.ps5.a with Ninja..."
"$ninja" -C "$build" src/amd/vulkan/libvulkan_radeon.a > "$build.log" 2>&1 ||
    { grep -E "error|FAILED" "$build.log" | head -20 >&2; exit 1; }

rm -rf "$install"
mkdir -p "$install/lib" "$install/include"
cp "$build/src/amd/vulkan/libvulkan_radeon.a" "$install/lib/libvulkan_radeon.ps5.a"
cp -r "$source_tree/include/vulkan" "$install/include/vulkan"
cp -r "$source_tree/include/vk_video" "$install/include/vk_video" 2>/dev/null || true
cat > "$install/PROVENANCE.txt" <<PROV
RADV for the PlayStation 5, built by tools/build-radv.sh $variant
fork: $mesa_fork
revision: $mesa_revision
assertions: $([[ $ndebug == true ]] && echo off || echo on)
sdk: $(cat "$sdk/.ps5-sdk-revision")
archive sha256: $(sha256sum "$install/lib/libvulkan_radeon.ps5.a" | cut -d' ' -f1)
PROV
ln -sf "$install" "$root/.deps/native/radv" 2>/dev/null || true
ln -sf "$install" "$root/.deps/native/radv-release" 2>/dev/null || true
echo "==> [radv] built RADV ($variant) at ${mesa_revision:0:12} into $install"
