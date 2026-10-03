#!/usr/bin/env bash
# UnleashedRecomp - Generate embedded BIN2C resources (.c / .h) from UnleashedRecompResources
# into $BUILD_DIR/res using tools/bin/file_to_c (matching UnleashedRecomp/CMakeLists.txt).

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
OUT_RES_DIR="$BUILD_DIR/res"
TOOLS_BIN="$ROOT/tools/bin"
FILE_TO_C="$TOOLS_BIN/file_to_c"

if [[ -f "$OUT_RES_DIR/bc_diff/button_bc_diff.bin.h" && -f "$OUT_RES_DIR/sounds/sys_actstg_pausewinopen.ogg.c" ]]; then
    exit 0
fi

chmod +x "$TOOLS_BIN/"* 2>/dev/null || true
if [[ ! -x "$FILE_TO_C" ]]; then
    bash "$ROOT/tools/build-host-tools.sh" >/dev/null
fi

RES_SRC_DIR="${UNLEASHED_RECOMP_RESOURCES:-}"
if [[ -z "$RES_SRC_DIR" ]]; then
    if [[ -d "$ROOT/UnleashedRecompResources/images" ]]; then
        RES_SRC_DIR="$ROOT/UnleashedRecompResources"
    elif [[ -d "$ROOT/.deps/UnleashedRecompResources/images" ]]; then
        RES_SRC_DIR="$ROOT/.deps/UnleashedRecompResources"
    elif [[ -d "/opt/ps5-cache/refs/UnleashedRecompResources/images" ]]; then
        RES_SRC_DIR="/opt/ps5-cache/refs/UnleashedRecompResources"
    else
        RES_SRC_DIR="$ROOT/.deps/UnleashedRecompResources"
        echo "==> [resources] Cloning https://github.com/hedge-dev/UnleashedRecompResources.git into $RES_SRC_DIR..."
        mkdir -p "$(dirname "$RES_SRC_DIR")"
        git clone --depth 1 https://github.com/hedge-dev/UnleashedRecompResources.git "$RES_SRC_DIR"
    fi
fi

echo "==> [resources] Generating 86 embedded BIN2C resource files into $OUT_RES_DIR..."
python3 - "$ROOT/UnleashedRecomp/CMakeLists.txt" "$RES_SRC_DIR" "$OUT_RES_DIR" "$FILE_TO_C" <<'PY'
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

cmake_file, res_src, out_res, file_to_c = sys.argv[1:]

with open(cmake_file, "r", encoding="utf-8") as f:
    content = f.read()

pattern = re.compile(
    r'BIN2C\(\s*TARGET_OBJ\s+\S+\s+SOURCE_FILE\s+"\$\{RESOURCES_SOURCE_PATH\}/([^"]+)"\s+'
    r'DEST_FILE\s+"\$\{RESOURCES_OUTPUT_PATH\}/([^"]+)"\s+ARRAY_NAME\s+"([^"]+)"'
    r'(?:\s+COMPRESSION_TYPE\s+"([^"]+)")?\s*\)'
)

tasks = []
for match in pattern.finditer(content):
    src_rel, dst_rel, array_name, comp_type = match.groups()
    comp = comp_type if comp_type else "none"
    src_path = os.path.join(res_src, src_rel)
    dst_base = os.path.join(out_res, dst_rel)
    os.makedirs(os.path.dirname(dst_base), exist_ok=True)
    tasks.append((file_to_c, src_path, array_name, comp, dst_base + ".c", dst_base + ".h"))

def run_one(t):
    subprocess.run(list(t), check=True)

with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
    list(ex.map(run_one, tasks))

print(f"    Generated {len(tasks)} resource pairs (.c + .h).")
PY
