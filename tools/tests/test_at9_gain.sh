#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
CC="${HOST_CC:-$(command -v gcc || command -v clang)}"
CXX="${HOST_CXX:-$(command -v g++ || command -v clang++)}"
TMP_ROOT="$ROOT/build/at9-test"
TMP_DIR="$TMP_ROOT/run-$$"
OBJ_DIR="$TMP_DIR/obj"
mkdir -p "$OBJ_DIR"
cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

for csrc in "$ROOT"/thirdparty/libatrac9/src/*.c; do
    "$CC" -O2 -c "$csrc" -o "$OBJ_DIR/$(basename "${csrc%.c}").o"
done

"$CXX" -std=c++20 -O2 \
    -I"$ROOT/thirdparty/stb" \
    -I"$ROOT/thirdparty/libatrac9/src" \
    "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" \
    "$OBJ_DIR"/*.o -lm -o "$TMP_DIR/wav_to_at9"

python3 - "$TMP_DIR/source.wav" <<'PY'
import math
import struct
import sys
import wave

path = sys.argv[1]
rate = 48000
frames = rate * 2
with wave.open(path, "wb") as audio:
    audio.setnchannels(2)
    audio.setsampwidth(2)
    audio.setframerate(rate)
    samples = bytearray()
    for i in range(frames):
        left = int(0.8 * 32767 * math.sin(2 * math.pi * 440 * i / rate))
        right = int(0.6 * 32767 * math.sin(2 * math.pi * 660 * i / rate))
        samples.extend(struct.pack("<hh", left, right))
    audio.writeframes(samples)
PY

"$TMP_DIR/wav_to_at9" "$TMP_DIR/source.wav" "$TMP_DIR/output.at9" 174

"$CXX" -std=c++20 -O2 \
    -I"$ROOT/thirdparty/stb" \
    -I"$ROOT/thirdparty/libatrac9/src" \
    "$ROOT/tools/tests/test_at9_gain.cpp" \
    "$OBJ_DIR"/*.o -lm -o "$TMP_DIR/test_at9_gain"
"$TMP_DIR/test_at9_gain" "$TMP_DIR/source.wav" "$TMP_DIR/output.at9"

# Exercise the asset pipeline's preserved ATRAC9 fallback: it must be copied
# byte-for-byte rather than decoded/re-encoded.
if [[ -f "$ROOT/sce_sys/snd0.source.at9" && -f "$ROOT/sce_sys/icon0.png" ]]; then
    prepare_fixture() {
        local fixture="$1"
        mkdir -p "$fixture/tools/wav_to_at9" "$fixture/tools/bin" "$fixture/sce_sys"
        cp "$ROOT/tools/prepare-assets.sh" "$fixture/tools/prepare-assets.sh"
        cp "$ROOT/tools/validate-assets.sh" "$fixture/tools/validate-assets.sh"
        cp "$ROOT/tools/wav_to_at9/wav_to_at9.cpp" "$fixture/tools/wav_to_at9/wav_to_at9.cpp"
        cp "$ROOT/sce_sys/icon0.png" "$fixture/sce_sys/icon0.png"
        cp "$ROOT/sce_sys/snd0.source.at9" "$fixture/sce_sys/snd0.source.at9"
    }

    FALLBACK_ROOT="$TMP_DIR/fallback-project"
    prepare_fixture "$FALLBACK_ROOT"
    fallback_log="$(bash "$FALLBACK_ROOT/tools/prepare-assets.sh" 2>&1)"
    printf '%s\n' "$fallback_log"
    cmp -s "$FALLBACK_ROOT/sce_sys/snd0.source.at9" "$FALLBACK_ROOT/sce_sys/snd0.at9" || {
        echo "AT9 fallback test: preserved source was not copied byte-for-byte." >&2
        exit 1
    }
    echo "AT9 fallback test: preserved source copied byte-for-byte."

    # An editable WAV must take precedence over the preserved .at9 fallback.
    EDITABLE_ROOT="$TMP_DIR/editable-project"
    prepare_fixture "$EDITABLE_ROOT"
    cp "$TMP_DIR/source.wav" "$EDITABLE_ROOT/sce_sys/snd0.wav"
    cp "$TMP_DIR/wav_to_at9" "$EDITABLE_ROOT/tools/bin/wav_to_at9"
    editable_log="$(bash "$EDITABLE_ROOT/tools/prepare-assets.sh" 2>&1)"
    printf '%s\n' "$editable_log"
    grep -Fq "Auto-selected audio source: $EDITABLE_ROOT/sce_sys/snd0.wav" <<<"$editable_log" || {
        echo "AT9 source-selection test: editable WAV did not take precedence over the fallback." >&2
        exit 1
    }
    "$TMP_DIR/test_at9_gain" "$TMP_DIR/source.wav" "$EDITABLE_ROOT/sce_sys/snd0.at9"
fi
