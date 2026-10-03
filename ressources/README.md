# Sonic Unleashed Retail Resources (`ressources/`)

Place your extracted Xbox 360 Sonic Unleashed directories inside this `ressources/` folder:

```text
ressources/
├── game/          # Base game files (must include default.xex and shader.ar)
├── update/        # Title Update files (must include default.xexp)
└── dlc/           # Optional Adventure Pack DLC folders
```

## Building & Recompiling for PlayStation 5 (`PPSA99902`)

Once `ressources/game`, `ressources/update`, and `ressources/dlc` are populated, run from the repository root:

```bash
make
```

What `make` does automatically:
1. **`tools/recomp-xex.sh`**:
   - Detects `ressources/game/default.xex`, `ressources/update/default.xexp`, and `ressources/game/shader.ar`.
   - Runs `XenonRecomp` on `default.xex` + `default.xexp` (`UnleashedRecompLib/config/SWA.toml`) to generate `UnleashedRecompLib/private/default_patched.xex`, stages `ressources/patched/default.xex`, and emits all 261 recompiled PowerPC C++ translation units (`UnleashedRecompLib/ppc/ppc_recomp.0.cpp` .. `ppc_recomp.260.cpp`).
   - Runs `x_decompress` on `shader.ar` to produce `shader_decompressed.ar`.
   - Runs `XenosRecomp` (built with `-DUNLEASHED_RECOMP`) on `default_patched.xex` + `shader_decompressed.ar` to compile all Xenos shaders via DXC into SPIR-V (`UnleashedRecompLib/shader/shader_cache.cpp`).
2. **`tools/build.sh`**:
   - Compiles `UnleashedRecompLib` and `UnleashedRecomp` for PlayStation 5 (`-target x86_64-sie-ps5`) in parallel across all CPU cores using precompiled headers (`ppc_recomp_shared.h.pch` and `stdafx.h.pch`).
   - Statically links Plume + PS5 RADV (`libvulkan_radeon.ps5.a`) + PacBrew (`SDL2`, `SDL2_mixer`, `libvorbisfile`, `libvorbis`, `libogg`, `libzstd`, `libiconv`).
   - Signs `dist/PPSA99902/eboot.bin` and stages `ressources/` into `dist/PPSA99902/ressources/`.
