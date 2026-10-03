#!/usr/bin/env bash
set -euo pipefail

# Sonic Unleashed Recompiled - Retail XEX & Shader Recompilation Pipeline
#
# Automatically detects game files placed in:
#   1. ./ressources/ (containing game/, update/, dlc/)
#   2. ./resources/  (containing game/, update/, dlc/)
#   3. A custom directory passed as $1
#   4. ./UnleashedRecompLib/private/ (containing default.xex, default.xexp, shader.ar)
#
# Workflow executed when retail files are detected:
#   1. Copies game/default.xex, update/default.xexp, and game/shader.ar into UnleashedRecompLib/private/
#   2. Runs XenonRecomp on UnleashedRecompLib/config/SWA.toml to patch default.xex + default.xexp
#      into default_patched.xex (also staged into ressources/patched/default.xex) and generate
#      UnleashedRecompLib/ppc/ppc_recomp.0.cpp .. ppc_recomp.260.cpp + ppc_func_mapping.cpp
#   3. Runs x_decompress on shader.ar -> shader_decompressed.ar
#   4. Runs XenosRecomp on default_patched.xex + shader_decompressed.ar -> UnleashedRecompLib/shader/shader_cache.cpp

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PRIVATE_DIR="${ROOT}/UnleashedRecompLib/private"
TOOLS_BIN="${ROOT}/tools/bin"

mkdir -p "${PRIVATE_DIR}"
chmod +x "${TOOLS_BIN}/"* 2>/dev/null || true

# Locate source directory containing game/ and update/
SRC_ROOT=""
if [[ $# -ge 1 && -n "${1:-}" && -d "$1" ]]; then
    SRC_ROOT="$1"
elif [[ -d "${ROOT}/ressources/game" ]]; then
    SRC_ROOT="${ROOT}/ressources"
elif [[ -d "${ROOT}/resources/game" ]]; then
    SRC_ROOT="${ROOT}/resources"
elif [[ -d "${ROOT}/game" ]]; then
    SRC_ROOT="${ROOT}"
fi

find_case_insensitive() {
    local dir="$1"
    local name="$2"
    [[ -d "$dir" ]] || return 1
    find "$dir" -maxdepth 2 -type f -iname "$name" -print -quit 2>/dev/null
}

if [[ -n "$SRC_ROOT" ]]; then
    echo "==> [recomp-xex] Scanning game resources in: ${SRC_ROOT}"
    GAME_XEX="$(find_case_insensitive "${SRC_ROOT}/game" "default.xex" || true)"
    UPDATE_XEXP="$(find_case_insensitive "${SRC_ROOT}/update" "default.xexp" || true)"
    GAME_SHADER_AR="$(find_case_insensitive "${SRC_ROOT}/game" "shader.ar" || true)"

    if [[ -n "$GAME_XEX" && -f "$GAME_XEX" ]]; then
        if [[ ! -f "${PRIVATE_DIR}/default.xex" || "$GAME_XEX" -nt "${PRIVATE_DIR}/default.xex" ]]; then
            echo "    Copying ${GAME_XEX} -> ${PRIVATE_DIR}/default.xex"
            cp -f "$GAME_XEX" "${PRIVATE_DIR}/default.xex"
            rm -f "${PRIVATE_DIR}/default_patched.xex"
        fi
    fi

    if [[ -n "$UPDATE_XEXP" && -f "$UPDATE_XEXP" ]]; then
        if [[ ! -f "${PRIVATE_DIR}/default.xexp" || "$UPDATE_XEXP" -nt "${PRIVATE_DIR}/default.xexp" ]]; then
            echo "    Copying ${UPDATE_XEXP} -> ${PRIVATE_DIR}/default.xexp"
            cp -f "$UPDATE_XEXP" "${PRIVATE_DIR}/default.xexp"
            rm -f "${PRIVATE_DIR}/default_patched.xex"
        fi
    fi

    if [[ -n "$GAME_SHADER_AR" && -f "$GAME_SHADER_AR" ]]; then
        if [[ ! -f "${PRIVATE_DIR}/shader.ar" || "$GAME_SHADER_AR" -nt "${PRIVATE_DIR}/shader.ar" ]]; then
            echo "    Copying ${GAME_SHADER_AR} -> ${PRIVATE_DIR}/shader.ar"
            cp -f "$GAME_SHADER_AR" "${PRIVATE_DIR}/shader.ar"
            rm -f "${PRIVATE_DIR}/shader_decompressed.ar" "${PRIVATE_DIR}/.recomp_shader_done"
        fi
    elif [[ -f "${SRC_ROOT}/game/#shader.ar.00" ]]; then
        if [[ ! -f "${PRIVATE_DIR}/shader.ar" || "${SRC_ROOT}/game/#shader.ar.00" -nt "${PRIVATE_DIR}/shader.ar" ]]; then
            echo "    Concatenating ${SRC_ROOT}/game/#shader.ar.* -> ${PRIVATE_DIR}/shader.ar"
            cat "${SRC_ROOT}/game/#shader.ar."* > "${PRIVATE_DIR}/shader.ar"
            rm -f "${PRIVATE_DIR}/shader_decompressed.ar" "${PRIVATE_DIR}/.recomp_shader_done"
        fi
    fi

    # Auto-normalize extracted DLC directories in ${SRC_ROOT}/dlc/ by inspecting DLC.xml <Type>N</Type>
    if [[ -d "${SRC_ROOT}/dlc" ]]; then
        while IFS= read -r -d '' dlc_xml; do
            dlc_dir="$(dirname "$dlc_xml")"
            dlc_type="$(sed -n 's/.*<Type>\([0-9]\)<\/Type>.*/\1/p' "$dlc_xml" | head -n 1 | tr -d '\r\n' || true)"
            target_name=""
            case "$dlc_type" in
                1) target_name="Spagonia Adventure Pack" ;;
                2) target_name="Chun-nan Adventure Pack" ;;
                3) target_name="Mazuri Adventure Pack" ;;
                4) target_name="Holoska Adventure Pack" ;;
                5) target_name="Apotos & Shamar Adventure Pack" ;;
                7) target_name="Empire City & Adabat Adventure Pack" ;;
            esac
            if [[ -n "$target_name" ]]; then
                target_dir="${SRC_ROOT}/dlc/${target_name}"
                if [[ "$dlc_dir" != "$target_dir" && ! -d "$target_dir" ]]; then
                    echo "    Organizing DLC (Type ${dlc_type}): $(basename "$dlc_dir") -> dlc/${target_name}"
                    mv "$dlc_dir" "$target_dir"
                fi
            fi
        done < <(find "${SRC_ROOT}/dlc" -mindepth 2 -maxdepth 2 -type f -iname "DLC.xml" -print0 2>/dev/null)
    fi
fi

if [[ -f "${PRIVATE_DIR}/default.xex" && -f "${PRIVATE_DIR}/shader.ar" ]]; then
    # Ensure host tools and libdxcompiler.so are available
    bash "${ROOT}/tools/build-host-tools.sh"
    if [[ -n "${DXC_ROOT:-}" && -d "${DXC_ROOT}/lib/x64" ]]; then
        DXC_LIB_DIR="${DXC_ROOT}/lib/x64"
    elif [[ -d "${ROOT}/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64" ]]; then
        DXC_LIB_DIR="${ROOT}/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64"
    else
        DXC_LIB_DIR="${ROOT}/.deps/dxc-bin/lib/x64"
    fi
    export LD_LIBRARY_PATH="${DXC_LIB_DIR}:${LD_LIBRARY_PATH:-}"

    # If default.xexp was not provided (e.g. default.xex is already patched), copy default.xex to default_patched.xex
    if [[ ! -f "${PRIVATE_DIR}/default.xexp" && ! -f "${PRIVATE_DIR}/default_patched.xex" ]]; then
        echo "==> [recomp-xex] No default.xexp found; treating default.xex as pre-patched."
        cp -f "${PRIVATE_DIR}/default.xex" "${PRIVATE_DIR}/default_patched.xex"
    fi

    if [[ "$(stat -c %s "${ROOT}/UnleashedRecompLib/ppc/ppc_recomp.0.cpp" 2>/dev/null || echo 0)" -lt 100000 || \
          ! -f "${ROOT}/UnleashedRecompLib/ppc/ppc_recomp.1.cpp" || \
          ! -f "${PRIVATE_DIR}/.recomp_ppc_done" || \
          "${PRIVATE_DIR}/default.xex" -nt "${PRIVATE_DIR}/.recomp_ppc_done" ]]; then
        echo "==> [recomp-xex] Running XenonRecomp on ${PRIVATE_DIR}/default.xex..."
        "${TOOLS_BIN}/XenonRecomp" \
            "${ROOT}/UnleashedRecompLib/config/SWA.toml" \
            "${ROOT}/tools/XenonRecomp/XenonUtils/ppc_context.h"
        touch "${PRIVATE_DIR}/.recomp_ppc_done"
    else
        echo "==> [recomp-xex] PowerPC C++ sources (ppc_recomp.*.cpp) are up to date."
    fi

    # Stage default_patched.xex into ressources/patched/default.xex so PS5 runtime boots immediately
    if [[ -n "$SRC_ROOT" && -f "${PRIVATE_DIR}/default_patched.xex" ]]; then
        mkdir -p "${SRC_ROOT}/patched"
        if [[ ! -f "${SRC_ROOT}/patched/default.xex" || "${PRIVATE_DIR}/default_patched.xex" -nt "${SRC_ROOT}/patched/default.xex" ]]; then
            cp -f "${PRIVATE_DIR}/default_patched.xex" "${SRC_ROOT}/patched/default.xex"
            echo "==> [recomp-xex] Staged patched executable to ${SRC_ROOT}/patched/default.xex"
        fi
    fi

    if [[ ! -f "${PRIVATE_DIR}/shader_decompressed.ar" || \
          "${PRIVATE_DIR}/shader.ar" -nt "${PRIVATE_DIR}/shader_decompressed.ar" ]]; then
        echo "==> [recomp-xex] Decompressing ${PRIVATE_DIR}/shader.ar -> ${PRIVATE_DIR}/shader_decompressed.ar..."
        "${TOOLS_BIN}/x_decompress" \
            "${PRIVATE_DIR}/shader.ar" \
            "${PRIVATE_DIR}/shader_decompressed.ar"
    fi

    if [[ "$(stat -c %s "${ROOT}/UnleashedRecompLib/shader/shader_cache.cpp" 2>/dev/null || echo 0)" -lt 4096 || \
          ! -f "${PRIVATE_DIR}/.recomp_shader_done" || \
          "${PRIVATE_DIR}/shader_decompressed.ar" -nt "${PRIVATE_DIR}/.recomp_shader_done" ]]; then
        echo "==> [recomp-xex] Running XenosRecomp to generate SPIR-V shader_cache.cpp..."
        SHADER_STAGE_DIR="${PRIVATE_DIR}/.shader_stage"
        rm -rf "${SHADER_STAGE_DIR}"
        mkdir -p "${SHADER_STAGE_DIR}"
        ln -sf "${PRIVATE_DIR}/default_patched.xex" "${SHADER_STAGE_DIR}/default_patched.xex"
        ln -sf "${PRIVATE_DIR}/shader_decompressed.ar" "${SHADER_STAGE_DIR}/shader_decompressed.ar"

        "${TOOLS_BIN}/XenosRecomp" \
            "${SHADER_STAGE_DIR}" \
            "${ROOT}/UnleashedRecompLib/shader/shader_cache.cpp" \
            "${ROOT}/tools/XenosRecomp/XenosRecomp/shader_common.h"

        rm -rf "${SHADER_STAGE_DIR}"
        touch "${PRIVATE_DIR}/.recomp_shader_done"
    else
        echo "==> [recomp-xex] Xenos shader cache (shader_cache.cpp) is up to date."
    fi

    echo "==> [recomp-xex] Retail PowerPC & Xenos shader recompilation complete!"
else
    echo "==> [recomp-xex] No retail files found in ressources/game/ (or ${PRIVATE_DIR}/); building with stub UnleashedRecompLib."
    echo "    (Drop game/, update/, and dlc/ into ./ressources/ and run 'make' to recompile retail Sonic Unleashed.)"
fi
