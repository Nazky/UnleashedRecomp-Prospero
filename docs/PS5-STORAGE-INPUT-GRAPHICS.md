# PS5 storage, controller, and Vulkan notes

Updated 2026-10-06. This note records the scope and safety decisions for the PS5 port changes.

## User-data location and migration

The non-portable PS5 default is `/data/UnleashedRecomp/`. Startup probes this exact directory by creating, writing, closing, and deleting a temporary file. If the probe fails, startup makes one bounded request through the cooperative Lapy protocol, then probes again. If `/data` is still unavailable, `GetUserPath()` uses the executable root (`/app0` in the packaged title). An existing `/app0/portable.txt` keeps its prior behavior and makes the executable root the user-data path directly.

When `/data` becomes writable, migration copies only `config.toml`, `cpkredir.ini`, `.ach_notif_restored`, and files beneath `save/` and `mlsave/` from the legacy roots used by this port. Existing files are never overwritten; symlinks and unrelated application assets are not copied. A `.legacy-user-data-migrated-v1` marker prevents repeat scans after a completed migration. The executable-root fallback is not assumed to be writable; it is the requested last-resort location.

The PS5 title metadata already sets `downloadDataSize` to a non-zero value, which is required by the upstream Lapy request-file mechanism.

## Lapy elevation: optional, upstream-only, firmware-sensitive

The client first cooperates with a resident daemon through `/download0/elevate_proc`, verifies real `/data` write/read/delete access, and only then selects `/data`. If no resident daemon claims the request, an optional one-shot upstream helper can be sent to the local ELF loader on port 9021. The application contains no local kernel-pointer mutation implementation.

The helper is intentionally **not part of the default package**. To build and package it, both opt-ins are required:

```sh
PACKAGE_LAPY_HELPER=1 ACK_UNVALIDATED_LAPY_HELPER=1 ./tools/build.sh
```

The helper build pins upstream `mpereiraesaa/PS5-Lapy-JB-Daemon` revision `5b8397b9f2b5f12a7bc2f9c8745a00d1c2dd01ad`, targets this title ID (`PPSA99902`), checks the upstream manifest and protocol hashes, and packages `lapy.elf`, its manifest, and its license. The explicit acknowledgement is required because the pinned manifest marks this exact-title helper as `console_validated: false`. It lists firmware 12.02 in its validated-firmware metadata, and the upstream project reports console testing of daemon builds there; that does **not** validate this title's generated one-shot helper. Other firmware remains experimental. Passing the runtime preflight is not a substitute for repeated launch/exit testing on the target console. Do not opt in unless you understand and accept that firmware-specific kernel-level risk. Without a helper or successful resident service, the application falls back to the executable root.

References:

- [PS5-Lapy-JB-Daemon](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon), pinned revision above.
- [ProsperoEden elevation integration notes](https://github.com/blackbearreloaded/ProsperoEden/tree/da6fd0eeaa762c52b7c4e9356adb7b1dfbbeba36/headless/elevation), used as the resident-first protocol reference.

## DualSense rumble

The native PS5 path now prefers DualSense audio haptics when the vibration audio port is available: it opens `sceAudioOut` port type 10 at 48 kHz with 256-frame stereo output, tries S16 then F32 PCM, selects `scePadSetVibrationMode(handle, 1)`, and streams synthesized low thumps plus filtered, decorrelated noise from a dedicated worker. There is no startup test burst; the stream follows game/menu rumble only. Failed port opens are retried at a throttled interval after successful pad polls, so a USB connection added after launch can be detected. If the port cannot open, mode 1 is rejected, the worker cannot start, or output repeatedly fails, the code attempts to select compatible mode 2 and resumes `scePadSetVibration` when the pad API accepts it.

XInput's left/strong motor drives the PS5 large actuator and the right/weak motor drives the small actuator. The selected route slider is the normal gameplay gain: `Rumble Strength` scales SDL/DS4 and compatible-mode PS5 motor output, while PS5 `Vibration Strength` scales the advanced type-10 audio-haptics route. The `sub_82B4DB48` game hook identifies the Boost filter envelope (`f1 > 0`); while active, PS5 game rumble uses twice the selected route strength, capped at 100%. Menu pulses are not doubled. The XAM vibration call exposes only motor strengths, not action IDs; the workspace also has no game XEX in `ressources/game` to inspect. Thus separate combat, impact, and other in-game event counts cannot be verified here: they arrive at this layer as generic motor values. Any unrelated rumble overlapping the Boost-active window receives the same Boost multiplier. Both sliders range from 0% to 100% and default to 100%. Menu pulses use the slider corresponding to their active output route; when `Vibration Menu` is enabled, changing either strength slider also triggers a brief preview pulse scaled by that setting. SDL/DS4 does not get the PS5-only Boost multiplier. Native `scePad` remains preferred over SDL's active-device classification. Logs report Boost-gain activation/deactivation, the selected mode/route, port/format, first nonzero PCM request or accepted motor request, and fallback errors. API success only proves that a request was accepted, not that the physical haptics moved. The new type-10 audio route still requires a clean PS5 build and wired/wireless hardware validation. Adaptive triggers are not part of this change.

Reference: [mihawk-99/PS5_RetroArch PR #6](https://github.com/mihawk-99/PS5_RetroArch/pull/6) and its [controller implementation](https://github.com/mihawk-99/PS5_RetroArch/blob/main/src/input_ps5.cpp).

## Graphics and performance: what is exposed, and what is deliberately unchanged

The title continues to present at 3840x2160. The existing `ResolutionScale` option changes internal rendering resolution; it is not an output-mode switch. The checked-in Plume surface creation requests the 3840x2160 Vulkan display mode, and the PS5 Mesa WSI implementation is centered on the console's 4K VideoOut buffers. A user-facing 1080p/1440p output selector was therefore not added: changing the window dimensions would not reliably change the physical VideoOut mode.

Changes made without altering the game's default visual settings:

- On PS5, the FPS setting is a discrete left/right choice of 30, 60, 90, or 120 FPS; 60 FPS remains the default. Existing config values are normalized to the nearest supported choice when loaded. This changes the software frame-pacing target only; it does not change the 4K output mode, display refresh mode, or high-frame-rate title metadata, and actual presentation remains subject to VSync and the display.
- The PS5 video menu exposes anisotropic filtering as five discrete choices: None (0), 2x, 4x, 8x, or 16x; 16x remains the default. Legacy values are normalized to the nearest supported choice. The existing depth-of-field selector remains available (`Auto` stays the default; lower blur kernels can reduce work).
- The PC-only Fullscreen selector is hidden on PS5. The controller guide remains on the existing PlayStation glyph region; this archive snapshot has no controller texture source to inspect and no verified PS5-specific sprite region, so no icons or art assets were changed.
- VSync-off requests immediate presentation only if the pinned PS5 Vulkan driver advertises that present mode; changing VSync requires swapchain recreation. A 60 Hz display cannot show more than 60 unique refreshes, though immediate/tearing presentation can in principle let rendering or present submissions exceed 60 FPS. The user's reported 60 FPS with VSync off has not been independently measured on-console, so this project does not claim the off-mode FPS cap is verified.
- Existing `ResolutionScale`, shadow, and MSAA options remain in place. No renderer backend, shader path, or default setting was replaced.
- Triple-buffer selection remains at its existing `Auto` default and is not exposed as a live menu setting: this port's PS5 restart prompt performs an in-process game soft reboot, not a Vulkan device/swapchain recreation. Exposing a setting that only takes effect after a full app relaunch would be misleading.

The project `tools/build-radv.sh` pins Mesa revision `0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8`. The current PS5_Vulkan upstream has since advanced and documents optional `RADV_THREADED_RECORDING=1` command-buffer recording, off by default and still requiring game-level performance measurement. That newer feature was not added to this older pinned driver or toggled from the title; changing the Mesa pin without building and testing the whole title on a console could regress the working renderer.

The project's pinned Mesa WSI only offers the native 4K output mode, with a 59.94 Hz default and a conditional 119.88 Hz path tied to title metadata. The current `sce_sys/param.json` keeps `attribute3` at `0`. The pin and metadata were left unchanged: 120 Hz output requires matching the WSI revision's metadata interpretation, a display that actually sustains the mode, and game-level timing tests. Do not copy a high-frame-rate metadata bit from a different Mesa revision.

References:

- [This project's pinned RADV build script](../tools/build-radv.sh).
- [PS5_Vulkan current build script](https://github.com/mihawk-99/PS5_Vulkan/blob/main/tools/build-radv.sh) and [current project README](https://github.com/mihawk-99/PS5_Vulkan#the-radv-port), documenting the newer RADV branch and its validation status.
- [Pinned PS5 Mesa VideoOut WSI source](https://github.com/mihawk-99/PS5_Mesa/blob/0b2d6d1a61d9bbf89cf8beb88a696144f67c61f8/src/vulkan/wsi/wsi_common_videoout.c).

## Menu vibration and system-information panel

The Input-category `Vibration` setting is the master switch for gameplay rumble and all menu feedback. When it is on, `Rumble Strength` is shown first; on PS5, the `Vibration Strength` slider follows for the advanced audio-haptics route. Both range from 0% to 100% and default to 100%. The separate `Vibration Menu` toggle controls short pulses for custom options-menu row/category movement and adjustment as well as existing title/pause-menu feedback; it defaults to on to preserve existing behavior. A 55 ms cooldown limits duplicate requests. The menu pulse is set to 50% large-motor and 37.5% small-motor strength for 120 ms; it is not subject to the Boost multiplier, but it is scaled by the strength setting for the active route. Adjusting either strength slider also emits a brief preview pulse scaled by the newly selected percentage, provided `Vibration Menu` is on. On PS5, menu pulses mix with game rumble and restore its level after the pulse. Both use the selected native `scePad` route: type-10 PCM haptics when available, otherwise compatible-mode dual-motor rumble. Native routing is retained even when SDL reports an unknown PS5 controller. Route/mode and API results are logged; the user has confirmed conventional rumble works, while the added audio-port path still needs hardware retesting.

The Overlay-category `System Info Overlay` toggle draws a top-left panel titled from the package's `localizedParameters.defaultLanguage.titleName` in `sce_sys/param.json` (falling back to `titleId`, then a generic `GAME` label). It shows presented FPS, CPU update time, GPU name/frame time, CPU/GPU temperatures, indexed SoC temperatures, current/peak/capacity for the game's and physical O1Heap allocators, internal render resolution, user-data path, executable path, and executable-root/mount path. CPU update time is not CPU utilization; heap readings are allocator metrics, not total process RSS, free system RAM, or console-wide memory. Temperature reads refresh once per second: CPU uses `sceKernelGetCpuTemperature`; GPU uses SoC sensor 0 following [OnionHEN's overlay implementation](https://github.com/aydencharles/onionHEN/blob/main/source/shellui/src/prx_overlay.cpp). Sentinel-initialized outputs are validated as plausible Celsius readings; invalid readings and nonzero statuses log the status/raw output, while a plausible value is not suppressed only because the API status is nonzero. Unavailable temperatures keep their translated labels and show `Unavailable`. Other plausible sensor IDs remain labeled as SoC sensors. The accent is selected from a paginated 256-color preset palette (11 pages, 24 swatches per page except the last); the default remains Sonic blue `(0, 120, 247)`, and it colors the title/FPS text as well as the panel stripe/divider. Legacy `[Video]` overlay keys and RGB-to-preset migration remain supported. The portable ImGui renderer and integration notes are in `extras/SystemInfoOverlay/`. All visible settings/labels are translated in English, Japanese, German, French, Spanish, and Italian.

## Validation limits

This workspace does not contain CMake, SDL2 development headers, a PS5 Payload SDK, or a console, so no full desktop or PS5 build was run. For this overlay update, the portable renderer passed `g++ -std=c++17 -Wall -Wextra -Werror`; host stub tests verified `param.json` title selection, plausible CPU/GPU values even with nonzero API statuses, and legacy `[Video]` TOML migration; localization coverage was checked for all six languages. Earlier project checks also covered shell-script syntax, localization-initializer syntax, and the standalone elevation-client source. A signed PS5 build and live sensor/API-status retest, controller test, `/data` permission test, Lapy-helper run, output-mode test, and performance measurement still require the appropriate PS5 toolchain and hardware.
