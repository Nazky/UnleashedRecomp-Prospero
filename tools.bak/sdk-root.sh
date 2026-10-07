#!/usr/bin/env bash
# PS5 Vulkan compatibility probe - which ps5-opengl SDK tree the tools read.
# Copyright (C) 2026 Mihawk-99
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The SDK this repository builds its shader compiler, its AGC package writer and
# its Vulkan headers from is not vendored: it lives beside the checkout, and
# tools/adapt-opengl-sdk.sh reconstructs it from what the tools need. Since
# ps5-opengl-sdk-0.3.0's release layout is not the SDK layout the tools read,
# that reconstruction is the normal way to get a tree now (docs/BLOCKERS.md,
# the SDK section):
#
#   1. PS5_OPENGL_SDK, when set, names the tree to use, whatever its layout is.
#   2. .deps/native/opengl-sdk, the tree tools/adapt-opengl-sdk.sh writes.
#   3. ../ps5-opengl-sdk-0.2.0, the pinned SDK layout, for a checkout that still
#      has one.
#
# Sourced by every tool that reads the SDK, so the resolution is in one place.
# ps5vk_opengl_sdk ROOT prints the path; it does not check that it exists, so a
# missing tree stays the calling script's error to report.

ps5vk_opengl_sdk() {
    local root=$1
    if [[ -n ${PS5_OPENGL_SDK:-} ]]; then
        printf '%s\n' "$PS5_OPENGL_SDK"
    elif [[ -d $root/.deps/native/opengl-sdk/third_party/opengnm-psbc ]]; then
        printf '%s\n' "$root/.deps/native/opengl-sdk"
    else
        printf '%s\n' "$root/../ps5-opengl-sdk-0.2.0"
    fi
}
