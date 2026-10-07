#!/usr/bin/env bash
# PS5 Vulkan compatibility probe - Python with mako for Mesa's generators.
# Copyright (C) 2026 Mihawk-99
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Sourced by tools/build-vulkan-runtime.sh and tools/build-driver.sh. Mesa's
# Vulkan generators are mako templates. mesa_python_init makes python3 import
# mako and markupsafe: an importable copy, or PS5VK_MAKO_PATH naming a
# directory that provides them. It sets mako_version and returns 2 when
# nothing works.

mesa_python_init() {
    if ! python3 -c 'import mako, markupsafe' 2>/dev/null; then
        local candidate
        for candidate in ${PS5VK_MAKO_PATH:-}; do
            if [[ -d $candidate/mako ]] &&
                PYTHONPATH="$candidate" python3 -c 'import mako, markupsafe' 2>/dev/null; then
                export PYTHONPATH="$candidate${PYTHONPATH:+:$PYTHONPATH}"
                break
            fi
        done
        python3 -c 'import mako, markupsafe' 2>/dev/null ||
            { echo "Mesa's generators need mako and markupsafe; set PS5VK_MAKO_PATH" >&2; return 2; }
    fi
    mako_version=$(python3 -c 'import mako; print(mako.__version__)')
}
