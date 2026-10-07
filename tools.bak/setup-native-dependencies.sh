#!/usr/bin/env bash
# ps5-native-app-boilerplate - Native dependency bootstrapper.
# Installs the PS5 payload SDK (mihawk-99/PS5_PayloadSDK) and static zlib into .deps/native.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cache="$root/.deps/native"
sdk="$cache/ps5-payload-sdk"
zlib_directory="$cache/zlib"
zlib_root="$zlib_directory/root"
zlib_version="1.3.2"
zlib_source="$zlib_directory/zlib-$zlib_version"
zlib_archive="$zlib_directory/zlib-$zlib_version.tar.gz"
zlib_stamp="$zlib_root/.source-version"
sdk_revision=95c08f27386fc698f6bbe21dde3030140a41d10b
sdk_repo_url="https://github.com/mihawk-99/PS5_PayloadSDK.git"
zlib_url="https://zlib.net/fossils/zlib-$zlib_version.tar.gz"
zlib_hash="bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
skip_sdk=false

if [[ ${1:-} == "--skip-sdk" ]]; then
    skip_sdk=true
elif [[ $# -ne 0 ]]; then
    echo "usage: tools/setup-native-dependencies.sh [--skip-sdk]" >&2
    exit 2
fi

for command in git wget unzip sha256sum tar make; do
    command -v "$command" >/dev/null || {
        echo "missing required command: $command" >&2
        exit 2
    }
done
compiler=$(command -v clang || command -v clang-18 || command -v gcc || command -v cc || true)
archiver=$(command -v llvm-ar || command -v llvm-ar-18 || command -v ar || true)
ranlib=$(command -v llvm-ranlib || command -v llvm-ranlib-18 || command -v ranlib || true)
[[ -n $compiler && -n $archiver && -n $ranlib ]] || {
    echo "clang, an archive tool, and ranlib are required to build zlib" >&2
    exit 2
}

mkdir -p "$cache"

restore_sdk_permissions() {
    [[ -d $sdk/bin ]] || return 0
    find "$sdk/bin" -maxdepth 1 -type f -exec chmod +x {} +
}

restore_sdk_permissions
if ! $skip_sdk && [[ ! -f $sdk/.ps5-sdk-revision || $(<"$sdk/.ps5-sdk-revision") != "$sdk_revision" ]]; then
    sdk_fork="${PS5_PAYLOAD_SDK_FORK:-}"
    if [[ -z "$sdk_fork" ]]; then
        if [[ -d "$root/../PS5_PayloadSDK/.git" ]]; then
            sdk_fork="$root/../PS5_PayloadSDK"
        elif [[ -d "/opt/ps5-cache/refs/PS5_PayloadSDK/.git" ]]; then
            sdk_fork="/opt/ps5-cache/refs/PS5_PayloadSDK"
        else
            sdk_fork="$root/.deps/src/PS5_PayloadSDK"
        fi
    fi
    if [[ ! -d "$sdk_fork/.git" ]]; then
        echo "==> [sdk] Cloning $sdk_repo_url into $sdk_fork" >&2
        mkdir -p "$(dirname "$sdk_fork")"
        git clone "$sdk_repo_url" "$sdk_fork" >&2
    fi
    git -C "$sdk_fork" cat-file -e "$sdk_revision^{commit}" 2>/dev/null || {
        git -C "$sdk_fork" fetch origin "$sdk_revision" >&2 || true
    }
    sdk_tree=$(mktemp -d)
    git -C "$sdk_fork" archive "$sdk_revision" | tar -x -C "$sdk_tree"
    # The pinned SDK's include/platform install recipes expand DESTDIR into
    # unquoted shell words. Quote those paths so a project directory containing
    # spaces or parentheses (for example, "TestVibration (2)") can install.
    for sdk_makefile in "$sdk_tree/include/Makefile" "$sdk_tree/platform/Makefile"; do
        if [[ ! -f "$sdk_makefile" ]]; then
            echo "ERROR: missing pinned SDK install makefile: $sdk_makefile" >&2
            exit 2
        fi
        sed -i 's@\$(DESTDIR)@"$(DESTDIR)"@g' "$sdk_makefile"
    done
    bash "$sdk_tree/platform/tools/setup-sdk.sh" "$sdk" "$sdk_revision" "$cache" >&2
    rm -rf -- "$sdk_tree"
    restore_sdk_permissions
fi
if ! $skip_sdk && [[ ! -x "$sdk/bin/prospero-lld" || ! -d "$sdk/target/include" ]]; then
    echo "the pinned PS5 payload SDK is incomplete" >&2
    exit 2
fi

zlib_library=$(find "$zlib_root" -type f -name libz.a -print -quit 2>/dev/null || true)
if [[ -z $zlib_library || ! -f $zlib_root/usr/include/zlib.h ||
    ! -f $zlib_stamp || $(<"$zlib_stamp") != "$zlib_version" ]]; then
    echo "==> [deps] Building pinned zlib $zlib_version from source" >&2
    mkdir -p "$zlib_directory"
    if [[ -f $zlib_archive ]] &&
        ! printf '%s  %s\n' "$zlib_hash" "$zlib_archive" |
            sha256sum --check --strict >/dev/null 2>&1; then
        rm -f -- "$zlib_archive"
    fi
    if [[ ! -f $zlib_archive ]]; then
        wget -q "$zlib_url" -O "$zlib_archive.download"
        mv "$zlib_archive.download" "$zlib_archive"
    fi
    printf '%s  %s\n' "$zlib_hash" "$zlib_archive" | sha256sum --check --strict >/dev/null
    rm -rf -- "$zlib_source" "$zlib_root"
    tar -xzf "$zlib_archive" -C "$zlib_directory"
    jobs=${BUILD_JOBS:-$(nproc 2>/dev/null || printf '2')}
    # zlib 1.3.2's install recipes also leave DESTDIR unquoted. Install into a
    # space-free /tmp staging path, then copy the finished /usr tree into .deps.
    zlib_stage=$(mktemp -d /tmp/ps5-zlib-install.XXXXXX)
    (
        trap 'rm -rf -- "$zlib_stage"' EXIT
        cd "$zlib_source"
        CC="$compiler" AR="$archiver" RANLIB="$ranlib" ./configure --static --prefix=/usr
        make -j "$jobs" CC="$compiler" AR="$archiver" RANLIB="$ranlib"
        make DESTDIR="$zlib_stage" install
        mkdir -p "$zlib_root"
        cp -a "$zlib_stage/usr" "$zlib_root/"
    ) >"$zlib_directory/build.log"
    printf '%s\n' "$zlib_version" >"$zlib_stamp"
    zlib_library=$(find "$zlib_root" -type f -name libz.a -print -quit 2>/dev/null || true)
fi
if [[ -z $zlib_library ]]; then
    echo "the pinned native zlib archive was not found after compilation" >&2
    exit 2
fi

printf 'SDK_ROOT=%s\nZLIB_INCLUDE=%s\nZLIB_ARCHIVE=%s\n' \
    "$sdk" "$zlib_root/usr/include" "$zlib_library"
