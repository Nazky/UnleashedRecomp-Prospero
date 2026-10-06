#!/usr/bin/env bash
# ps5-native-app-boilerplate - Optional compiler cache for the heavy builds.
# Copyright (C) 2026 Mihawk-99
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The Mesa-derived archives recompile hundreds of C sources whenever the
# objects are missing: a cold PS5 shader compiler archive is about 53 s here
# and the driver's position-independent copy about 75 s. ccache keys each
# compile by preprocessed source and flags, so a repeat build replays cached
# objects instead of recompiling them.
#
# The cache lives in the ignored .deps tree beside the inputs it caches, so
# `make distclean` removes both together. ccache is optional: when the binary
# is absent, or when PS5VK_DISABLE_CCACHE is set, the compiler command comes
# back unchanged and the build behaves exactly as before.
#
# Usage:
#   source "$root/tools/ccache.sh"
#   cc=$(ps5vk_ccache "$payload/bin/prospero-clang")

_ps5vk_ccache_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
export CCACHE_DIR=${CCACHE_DIR:-$_ps5vk_ccache_root/.deps/ccache}

ps5vk_ccache() {
    local compiler=$1
    if [[ -z ${PS5VK_DISABLE_CCACHE:-} ]] && command -v ccache >/dev/null 2>&1; then
        printf 'ccache %s' "$compiler"
    else
        printf '%s' "$compiler"
    fi
}
