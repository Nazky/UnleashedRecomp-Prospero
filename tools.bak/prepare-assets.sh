#!/usr/bin/env bash
# UnleashedRecomp-Prospero - Native Linux PS5 Presentation Asset Converter (sce_sys/)
#
# Converts ordinary PNG/JPG artwork and WAV/MP3/OGG/FLAC/M4A/AAC audio into the
# PS5 sce_sys/ formats required by the launcher without requiring Windows
# texconv.exe or ps4_at9tool.exe:
#   - sce_sys/icon0.png : 512x512 PNG
#   - sce_sys/pic0.dds  : 3840x2160 BC7_UNORM (DXGI_FORMAT_BC7_UNORM = 98) DX10 2D DDS (Home Screen background)
#   - sce_sys/pic1.dds  : 3840x2160 BC7_UNORM (DXGI_FORMAT_BC7_UNORM = 98) DX10 2D DDS (Launch splash screen)
#   - sce_sys/snd0.at9  : 48 kHz looped ATRAC9 RIFF/WAVE (<= 2 MiB, Home Screen background music)
#
# Usage:
#   1) Auto-sync mode (called automatically by `make` / `tools/build.sh`):
#      Simply place/replace `sce_sys/pic0.png`, `sce_sys/pic1.png`, `sce_sys/icon0.png`,
#      or `sce_sys/snd0.{wav,mp3,ogg,flac,at9}` and run:
#        bash tools/prepare-assets.sh
#
#   2) Explicit CLI mode:
#        bash tools/prepare-assets.sh \
#            [--icon path/to/icon.png] \
#            [--background path/to/bg.png] \
#            [--selection-background path/to/pic0.png] \
#            [--launch-background path/to/pic1.png] \
#            [--audio path/to/music.{wav,mp3,ogg,flac,m4a,aac,at9}] \
#            [--audio-duration 87]

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
SCE_SYS="$ROOT/sce_sys"
TOOLS_BIN="$ROOT/tools/bin"
CONVERTER="$TOOLS_BIN/png_to_bc7_dds"
AT9_ENCODER="$TOOLS_BIN/wav_to_at9"

ICON_IN=""
BG_IN=""
PIC0_IN=""
PIC1_IN=""
AUDIO_IN=""
AUDIO_DURATION="174"
AT9_TOOL="${PS4_AT9TOOL:-}"
FORCE_SYNC=0

while (($# > 0)); do
    case "$1" in
        --icon)
            ICON_IN="${2:-}"; shift 2 ;;
        --background|--bg)
            BG_IN="${2:-}"; shift 2 ;;
        --selection-background|--pic0)
            PIC0_IN="${2:-}"; shift 2 ;;
        --launch-background|--splash|--pic1)
            PIC1_IN="${2:-}"; shift 2 ;;
        --audio|--music|--snd0)
            AUDIO_IN="${2:-}"; shift 2 ;;
        --audio-duration)
            AUDIO_DURATION="${2:-174}"; shift 2 ;;
        --at9-tool)
            AT9_TOOL="${2:-}"; shift 2 ;;
        --force|-f)
            FORCE_SYNC=1; shift ;;
        --help|-h)
            sed -n '2,26p' "${BASH_SOURCE[0]}"
            exit 0 ;;
        *)
            echo "Unknown option: $1" >&2
            exit 2 ;;
    esac
done

ensure_converter() {
    if [[ ! -x "$CONVERTER" || "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" -nt "$CONVERTER" ]]; then
        mkdir -p "$TOOLS_BIN"
        local cxx="${HOST_CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
        echo "==> [assets] Building native Linux BC7 DDS converter ($CONVERTER)..." >&2
        "$cxx" -std=c++20 -O3 -fopenmp \
            -I"$ROOT/thirdparty/stb" \
            "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" \
            -o "$CONVERTER" 2>/dev/null || \
        "$cxx" -std=c++20 -O3 \
            -I"$ROOT/thirdparty/stb" \
            "$ROOT/tools/png_to_bc7_dds/png_to_bc7_dds.cpp" \
            -o "$CONVERTER"
    fi
}

ensure_at9_encoder() {
    if [[ ! -x "$AT9_ENCODER" || "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" -nt "$AT9_ENCODER" ]]; then
        mkdir -p "$TOOLS_BIN"
        local cxx="${HOST_CXX:-$(command -v g++ || command -v clang++-18 || command -v clang++)}"
        local cc="${HOST_CC:-$(command -v gcc || command -v clang-18 || command -v clang)}"
        local build_at9="$ROOT/build/host-at9"
        echo "==> [assets] Building native Linux ATRAC9 encoder ($AT9_ENCODER)..." >&2
        mkdir -p "$build_at9"
        for csrc in "$ROOT"/thirdparty/libatrac9/src/*.c; do
            "$cc" -O3 -c "$csrc" -o "$build_at9/$(basename "${csrc%.c}").o"
        done
        "$cxx" -std=c++20 -O3 \
            -I"$ROOT/thirdparty/stb" \
            -I"$ROOT/thirdparty/libatrac9/src" \
            "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" \
            "$build_at9"/*.o -lm \
            -o "$AT9_ENCODER"
        rm -rf "$build_at9"
    fi
}

encode_audio_to_at9() {
    local src="$1"
    local dst="$2"
    case "${src,,}" in
        *.at9)
            if [[ "$(realpath "$src")" != "$(realpath -m "$dst")" ]]; then
                echo "==> [assets] Installing $src -> $dst..."
                cp -f "$src" "$dst"
            fi
            ;;
        *.wav|*.mp3|*.ogg|*.flac)
            if [[ -n "$AT9_TOOL" ]]; then
                command -v ffmpeg >/dev/null 2>&1 || { echo "Error: ffmpeg is required when using --at9-tool." >&2; exit 1; }
                local tmp_wav
                tmp_wav="$(mktemp --suffix=.wav)"
                ffmpeg -y -v error -i "$src" -vn -sn -dn -t "$AUDIO_DURATION" -ar 48000 -ac 2 -c:a pcm_s16le "$tmp_wav"
                echo "==> [assets] Encoding $src -> $dst via $AT9_TOOL..."
                if [[ "$AT9_TOOL" == *.exe ]] && command -v wine >/dev/null 2>&1 && ! grep -qi microsoft /proc/version 2>/dev/null; then
                    wine "$AT9_TOOL" -e -br 144 -wholeloop "$tmp_wav" "$dst"
                else
                    "$AT9_TOOL" -e -br 144 -wholeloop "$tmp_wav" "$dst"
                fi
                rm -f "$tmp_wav"
            else
                ensure_at9_encoder
                echo "==> [assets] Encoding $src -> $dst (48 kHz stereo looped ATRAC9, max ${AUDIO_DURATION}s)..."
                if ! "$AT9_ENCODER" "$src" "$dst" "$AUDIO_DURATION" 2>/dev/null; then
                    if command -v ffmpeg >/dev/null 2>&1; then
                        local tmp_wav
                        tmp_wav="$(mktemp --suffix=.wav)"
                        ffmpeg -y -v error -i "$src" -vn -sn -dn -t "$AUDIO_DURATION" -ar 48000 -ac 2 -c:a pcm_s16le "$tmp_wav"
                        "$AT9_ENCODER" "$tmp_wav" "$dst" "$AUDIO_DURATION"
                        rm -f "$tmp_wav"
                    else
                        "$AT9_ENCODER" "$src" "$dst" "$AUDIO_DURATION"
                    fi
                fi
            fi
            ;;
        *.m4a|*.aac|*.opus|*.wma)
            command -v ffmpeg >/dev/null 2>&1 || {
                echo "Error: Converting ${src} requires ffmpeg on PATH (or provide .wav, .mp3, .ogg, .flac, or .at9)." >&2
                exit 1
            }
            local tmp_wav
            tmp_wav="$(mktemp --suffix=.wav)"
            echo "==> [assets] Decoding $src to 48 kHz stereo PCM via ffmpeg..."
            ffmpeg -y -v error -i "$src" -vn -sn -dn -t "$AUDIO_DURATION" -ar 48000 -ac 2 -c:a pcm_s16le "$tmp_wav"
            if [[ -n "$AT9_TOOL" ]]; then
                echo "==> [assets] Encoding $src -> $dst via $AT9_TOOL..."
                "$AT9_TOOL" -e -br 144 -wholeloop "$tmp_wav" "$dst"
            else
                ensure_at9_encoder
                echo "==> [assets] Encoding $src -> $dst (48 kHz stereo looped ATRAC9)..."
                "$AT9_ENCODER" "$tmp_wav" "$dst" "$AUDIO_DURATION"
            fi
            rm -f "$tmp_wav"
            ;;
        *)
            echo "Unsupported audio extension for $src (expected .wav, .mp3, .ogg, .flac, .m4a, .aac, or .at9)" >&2
            exit 1
            ;;
    esac
}

mkdir -p "$SCE_SYS"

# 1. Explicit --icon
if [[ -n "$ICON_IN" ]]; then
    [[ -f "$ICON_IN" ]] || { echo "Icon file not found: $ICON_IN" >&2; exit 1; }
    ensure_converter
    echo "==> [assets] Converting $ICON_IN -> $SCE_SYS/icon0.png (512x512)..."
    "$CONVERTER" "$ICON_IN" "$SCE_SYS/icon0.png" 512 512
fi

# 2. Explicit --background / --selection-background / --launch-background
if [[ -n "$BG_IN" ]]; then
    PIC0_IN="$BG_IN"
    PIC1_IN="$BG_IN"
fi

if [[ -n "$PIC0_IN" ]]; then
    [[ -f "$PIC0_IN" ]] || { echo "Selection background not found: $PIC0_IN" >&2; exit 1; }
    ensure_converter
    if [[ "$(realpath "$PIC0_IN")" != "$(realpath -m "$SCE_SYS/pic0.png")" ]]; then
        "$CONVERTER" "$PIC0_IN" "$SCE_SYS/pic0.png" 3840 2160
    fi
    echo "==> [assets] Encoding $PIC0_IN -> $SCE_SYS/pic0.dds (3840x2160 BC7_UNORM DX10 DDS)..."
    "$CONVERTER" "$PIC0_IN" "$SCE_SYS/pic0.dds"
    if [[ ! -f "$SCE_SYS/pic1.dds" && -z "$PIC1_IN" ]]; then
        cp -f "$SCE_SYS/pic0.png" "$SCE_SYS/pic1.png"
        cp -f "$SCE_SYS/pic0.dds" "$SCE_SYS/pic1.dds"
    fi
fi

if [[ -n "$PIC1_IN" ]]; then
    [[ -f "$PIC1_IN" ]] || { echo "Launch background not found: $PIC1_IN" >&2; exit 1; }
    ensure_converter
    if [[ "$(realpath "$PIC1_IN")" != "$(realpath -m "$SCE_SYS/pic1.png")" ]]; then
        "$CONVERTER" "$PIC1_IN" "$SCE_SYS/pic1.png" 3840 2160
    fi
    echo "==> [assets] Encoding $PIC1_IN -> $SCE_SYS/pic1.dds (3840x2160 BC7_UNORM DX10 DDS)..."
    "$CONVERTER" "$PIC1_IN" "$SCE_SYS/pic1.dds"
    if [[ ! -f "$SCE_SYS/pic0.dds" && -z "$PIC0_IN" ]]; then
        cp -f "$SCE_SYS/pic1.png" "$SCE_SYS/pic0.png"
        cp -f "$SCE_SYS/pic1.dds" "$SCE_SYS/pic0.dds"
    fi
fi

# 3. Auto-sync sce_sys/pic0.png -> sce_sys/pic0.dds and sce_sys/pic1.png -> sce_sys/pic1.dds
if [[ -z "$PIC0_IN" && -f "$SCE_SYS/pic0.png" ]]; then
    if [[ "$FORCE_SYNC" == "1" || ! -f "$SCE_SYS/pic0.dds" || "$SCE_SYS/pic0.png" -nt "$SCE_SYS/pic0.dds" ]]; then
        ensure_converter
        echo "==> [assets] Encoding $SCE_SYS/pic0.png -> $SCE_SYS/pic0.dds (3840x2160 BC7_UNORM DX10 DDS)..."
        "$CONVERTER" "$SCE_SYS/pic0.png" "$SCE_SYS/pic0.dds"
    fi
fi

if [[ -z "$PIC1_IN" && -f "$SCE_SYS/pic1.png" ]]; then
    if [[ "$FORCE_SYNC" == "1" || ! -f "$SCE_SYS/pic1.dds" || "$SCE_SYS/pic1.png" -nt "$SCE_SYS/pic1.dds" ]]; then
        ensure_converter
        echo "==> [assets] Encoding $SCE_SYS/pic1.png -> $SCE_SYS/pic1.dds (3840x2160 BC7_UNORM DX10 DDS)..."
        "$CONVERTER" "$SCE_SYS/pic1.png" "$SCE_SYS/pic1.dds"
    fi
fi

if [[ -f "$SCE_SYS/pic0.dds" && ! -f "$SCE_SYS/pic1.dds" ]]; then
    cp -f "$SCE_SYS/pic0.dds" "$SCE_SYS/pic1.dds"
elif [[ -f "$SCE_SYS/pic1.dds" && ! -f "$SCE_SYS/pic0.dds" ]]; then
    cp -f "$SCE_SYS/pic1.dds" "$SCE_SYS/pic0.dds"
fi

# 4. Explicit --audio OR auto-sync sce_sys/snd0.{wav,mp3,ogg,flac,m4a,aac} -> sce_sys/snd0.at9
if [[ -n "$AUDIO_IN" ]]; then
    [[ -f "$AUDIO_IN" ]] || { echo "Audio file not found: $AUDIO_IN" >&2; exit 1; }
    encode_audio_to_at9 "$AUDIO_IN" "$SCE_SYS/snd0.at9"
else
    for ext in wav mp3 ogg flac m4a aac; do
        cand="$SCE_SYS/snd0.$ext"
        if [[ -f "$cand" ]]; then
            if [[ "$FORCE_SYNC" == "1" || ! -f "$SCE_SYS/snd0.at9" || "$cand" -nt "$SCE_SYS/snd0.at9" ]]; then
                encode_audio_to_at9 "$cand" "$SCE_SYS/snd0.at9"
            fi
            break
        fi
    done
fi

bash "$ROOT/tools/validate-assets.sh" "$SCE_SYS"
