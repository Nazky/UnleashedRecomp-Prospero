#!/usr/bin/env bash
# ps5-homebrew-dev-protocol - Title-aware controller sender.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compiles the launch or close controller for one title ID and sends it to the
# console's ELF loader.
#
# Usage: send-controller.sh <launch|close> <PPSAxxxxx> <host> <elf-port>
#
# Environment:
#   PS5_PAYLOAD_SDK              SDK root (default /opt/ps5-payload-sdk)
#   PS5_CONTROLLER_COMPILE_ONLY  1: build only and print the ELF path

set -euo pipefail

usage() {
    echo "usage: $0 <launch|close> <PPSAxxxxx> <host> <elf-port>" >&2
    exit 2
}

[[ $# -eq 4 ]] || usage
mode=$1
title=$2
host=$3
port=$4
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ -n ${PS5_PAYLOAD_SDK:-} ]]; then
    sdk=$PS5_PAYLOAD_SDK
elif [[ -x "$root/../.deps/native/ps5-payload-sdk/bin/prospero-clang" ]]; then
    sdk="$root/../.deps/native/ps5-payload-sdk"
else
    sdk=/opt/ps5-payload-sdk
fi
compile_only=${PS5_CONTROLLER_COMPILE_ONLY:-${PS5_INVESTIGATION_COMPILE_ONLY:-0}}

[[ $mode == launch || $mode == close ]] || { echo "invalid controller mode: $mode" >&2; exit 2; }
[[ $title =~ ^PPSA[0-9]{5}$ ]] || { echo "invalid title ID: $title" >&2; exit 2; }
[[ -x $sdk/bin/prospero-clang ]] || { echo "missing $sdk/bin/prospero-clang (set PS5_PAYLOAD_SDK)" >&2; exit 2; }
if [[ $compile_only != 1 ]]; then
    [[ -n $host ]] || { echo "missing host" >&2; exit 2; }
    [[ $port =~ ^[0-9]+$ ]] || { echo "invalid port: $port" >&2; exit 2; }
    command -v nc >/dev/null || { echo "missing nc (netcat)" >&2; exit 2; }
fi

output_dir=${TMPDIR:-/tmp}/ps5-homebrew-protocol
mkdir -p "$output_dir"
output="$output_dir/${mode}-${title}.elf"
libraries=(-lSceSystemService)
if [[ $mode == launch ]]; then
    libraries+=(-lSceUserService)
fi

"$sdk/bin/prospero-clang" -Wall -Werror \
    -DBOOTSTRAP_TITLE_ID="\"$title\"" \
    "${libraries[@]}" -o "$output" "$root/controllers/$mode.c"
if [[ $compile_only == 1 ]]; then
    echo "$output"
    exit 0
fi
timeout --signal=TERM 15s nc -q0 "$host" "$port" < "$output"
