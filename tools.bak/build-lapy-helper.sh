#!/usr/bin/env bash
# Build the optional, exact-title upstream Lapy one-shot ELF helper.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TITLE_ID="PPSA99902"
LAPY_REVISION="5b8397b9f2b5f12a7bc2f9c8745a00d1c2dd01ad"
LAPY_REPO_URL="https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon.git"
SDK_ROOT="${PS5_PAYLOAD_SDK:-$ROOT/.deps/native/ps5-payload-sdk}"
LAPY_SOURCE="${LAPY_SOURCE_DIR:-$ROOT/.deps/src/PS5-Lapy-JB-Daemon}"
OUTPUT="$ROOT/build/lapy-owned-helper"
UPSTREAM_OUTPUT="$LAPY_SOURCE/build/owned_root_helper-$TITLE_ID"

if [[ "${ACK_UNVALIDATED_LAPY_HELPER:-0}" != "1" ]]; then
    cat >&2 <<'EOF'
ERROR: the pinned upstream one-shot Lapy helper has not been validated for this
exact title on a console. Building it opts into an experimental kernel-level
filesystem helper. Review docs/PS5-STORAGE-INPUT-GRAPHICS.md, then rerun with
ACK_UNVALIDATED_LAPY_HELPER=1 if you accept that risk.
EOF
    exit 2
fi

[[ -x "$SDK_ROOT/bin/prospero-clang" ]] || {
    echo "ERROR: PS5 Payload SDK not found at $SDK_ROOT" >&2
    echo "Set PS5_PAYLOAD_SDK or run tools/setup-native-dependencies.sh first." >&2
    exit 2
}
python3 - "$ROOT/sce_sys/param.json" "$TITLE_ID" <<'PY'
import json, sys
from pathlib import Path
metadata = json.loads(Path(sys.argv[1]).read_text())
if metadata.get("titleId") != sys.argv[2]:
    raise SystemExit("ERROR: app title ID differs from the helper target")
if int(metadata.get("downloadDataSize", 0)) <= 0:
    raise SystemExit("ERROR: title metadata must enable /download0 for Lapy requests")
PY

if [[ ! -d "$LAPY_SOURCE/.git" ]]; then
    mkdir -p "$(dirname -- "$LAPY_SOURCE")"
    git clone --filter=blob:none --no-checkout "$LAPY_REPO_URL" "$LAPY_SOURCE"
fi
if ! git -C "$LAPY_SOURCE" cat-file -e "$LAPY_REVISION^{commit}" 2>/dev/null; then
    git -C "$LAPY_SOURCE" fetch origin "$LAPY_REVISION"
fi
git -C "$LAPY_SOURCE" reset --hard "$LAPY_REVISION"
git -C "$LAPY_SOURCE" clean -fdx
[[ "$(git -C "$LAPY_SOURCE" rev-parse HEAD)" == "$LAPY_REVISION" ]] || {
    echo "ERROR: Lapy checkout does not match the pinned revision." >&2
    exit 2
}

python3 - "$LAPY_SOURCE/source/lapy_elevation_protocol.h" \
    "$ROOT/UnleashedRecomp/ps5/lapy_elevation_protocol.h" <<'PY'
import hashlib, sys
from pathlib import Path
upstream = hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()
local = hashlib.sha256(Path(sys.argv[2]).read_bytes()).hexdigest()
if upstream != local:
    raise SystemExit("ERROR: local elevation client protocol header differs from pinned upstream")
PY

make -C "$LAPY_SOURCE" owned-helper \
    PS5_PAYLOAD_SDK="$SDK_ROOT" \
    TARGET_TITLE="$TITLE_ID" \
    LOGGING_CLIENT="$ROOT/vendor/ps5/lapy-logging"

python3 - "$UPSTREAM_OUTPUT" "$OUTPUT" "$TITLE_ID" \
    "$LAPY_SOURCE/source/lapy_elevation_protocol.h" "$LAPY_SOURCE/LICENSE" <<'PY'
import hashlib, json, shutil, sys
from pathlib import Path

source, output, title, protocol, license_file = map(Path, sys.argv[1:])
elf = source / "lapy.elf"
release_manifest = source / "lapy-manifest.json"
manifest = json.loads(release_manifest.read_text())

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

required = {
    "schema": "lapy-owned-build/1",
    "target_title": str(title),
    "mode": "elf-helper",
    "max_requests": 1,
    "service": False,
    "require_client_result": False,
    "console_validated": False,
}
for key, expected in required.items():
    if manifest.get(key) != expected:
        raise SystemExit(f"ERROR: manifest {key}={manifest.get(key)!r}; expected {expected!r}")
if "root_layout_probe_retry" not in manifest.get("features", []):
    raise SystemExit("ERROR: upstream helper lacks root_layout_probe_retry")
if not elf.is_file() or elf.read_bytes()[:6] != b"\x7fELF\x02\x01":
    raise SystemExit("ERROR: helper is not a little-endian ELF64")
if manifest.get("elf_sha256") != digest(elf):
    raise SystemExit("ERROR: helper ELF digest does not match upstream manifest")
if manifest.get("protocol_sha256") != digest(protocol):
    raise SystemExit("ERROR: helper protocol digest does not match upstream manifest")

output.mkdir(parents=True, exist_ok=True)
shutil.copy2(elf, output / "lapy.elf")
shutil.copy2(release_manifest, output / "lapy-manifest.json")
shutil.copy2(license_file, output / "LICENSE.Lapy")
print(f"Verified exact-title helper for {title}")
print(f"  build:  {manifest['build_id']}")
print(f"  sha256: {manifest['elf_sha256']}")
print("  note:   upstream manifest says console_validated=false for this build")
PY
