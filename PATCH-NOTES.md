# PS5 Controller Vibration Follow-up — 2026-10-07

## Changes

- Replaces the fixed PS5 5%/10% gains with the selected route slider: normal gameplay uses the slider directly, and game rumble doubles during Boost, capped at 100%. Boost doubling applies to both type-10 haptics and compatible-mode motor fallback, not to menu pulses or SDL/DS4 output.
- Adds saved `Input.RumbleStrength` and `Input.VibrationStrength` sliders, both 0–100% and defaulting to 100%. Rumble Strength controls SDL and compatible-mode motor output; on PS5, Vibration Strength separately controls the advanced type-10 DualSense audio-haptics route. Menu pulses use the strength setting for their active route; with `Vibration Menu` on, adjusting either slider emits a brief preview pulse scaled by its selected percentage. The controls appear only while master `Vibration` is enabled.
- Adds a saved `Input.VibrationMenu` toggle, defaulting to on. It appears below the strength sliders only while the master `Vibration` option is enabled, and gates menu/title/pause feedback and strength previews independently.
- Retains the DualSense haptic path referenced by [PS5_RetroArch PR #6](https://github.com/mihawk-99/PS5_RetroArch/pull/6): type-10 `sceAudioOut` PCM haptics in advanced mode, compatible-mode `scePadSetVibration` fallback, and serialized fallback mode/motor writes.

## Validation limits

- Static route/UI/localization checks and a standalone C++17 strength/Boost-scaling harness passed. No PS5 SDK build or console verification was performed for this follow-up.

---

# PS5 In-Game Achievement Overlay & Home Screen Audio Update — v10

This candidate aligns achievement feedback with the linked Prospero branch’s standard local in-game overlay and restores its native ATRAC9 conversion path without extra fixed gain or MP3-only attenuation.

## Changes

- Keeps the top-left system-info overlay, title lookup from `sce_sys/param.json`, the 256-color paginated accent palette, Overlay settings, and existing legacy `[Video]` setting/RGB migrations.
- Keeps **Simple** and **Advanced** modes. Simple shows only the game title and FPS. Advanced shows the full overlay with visibility switches for the other metric/path items; title and FPS remain visible and have no hide switches.
- Removes CPU and GPU temperature readings from the overlay and removes their visible Advanced-mode settings. The old config keys are retained as hidden compatibility fields and ignored by the renderer. The optional **Other SoC Sensors** setting still displays supported sensor IDs other than sensor 0.
- Groups Advanced metrics into localized **Performance**, **SoC Sensors**, **Memory**, **Display**, and **Paths** sections. The portable ImGui renderer draws section headings/dividers and a modern dark card with an accent stripe, border, and shadow.
- Keeps the game title derived from `sce_sys/param.json` and preserves the top-left placement.
- Restores `tools/wav_to_at9/wav_to_at9.cpp` from the linked Prospero source (commit `465f247cbc0b671de7ab7f542458cc575cab1fdb`). Editable audio uses that upstream converter: no extra fixed gain or MP3-only attenuation; its fades and `-1 dBFS` peak-safety ceiling remain. Auto-sync still prefers the newest editable `sce_sys/snd0.*` source; if none exists, preserved `sce_sys/snd0.source.at9` is copied byte-for-byte instead of being lossy re-encoded. The generated `.at9` is never used as its own input. The shipped file is PS5 Home Screen background music, not the game's runtime XAudio stream.
- The build validates and compares `snd0.at9` in both `dist/<TITLE_ID>/sce_sys/` and `pkg/<TITLE_ID>/sce_sys/`. This verifies local staged bytes only, not the console's `/user/app/<TITLE_ID>/sce_sys/` installation.
- Replaces the custom PS5 top-right trophy-styled toast with the linked Prospero branch’s standard local achievement overlay: centered near the top, using the existing achievement icon, localized unlock label, achievement name, source animation/timing, and in-game sound.
- The unlock path records achievements locally first, then `Achievement Notifications` controls whether an overlay is queued. The setting is visible on PS5 and desktop; turning it off does not erase unlock records or pause-menu history.
- Keeps all feedback in-game only. There are no PS5 system-notification, PSN trophy-award, Trophy2/NPBind, or network-sync calls. The queue is mutex-protected; on PS5 only the native-main-thread identity check is bypassed so the standard overlay can draw from the guest game thread. The upstream audio-readiness/volume checks remain.
- Uses the active game text language for the existing localized “Achievement Unlocked” label and XDBF achievement name. English, Japanese, German, French, Spanish, and Italian are supported.
- Retains the default `/data/UnleashedRecomp/` user-data path and executable-root fallback if `/data` is not writable. Temperature readings remain disabled.

## Validation performed

- The standard overlay drawing was compared with `Nazky/UnleashedRecomp-Prospero` commit `465f247cbc0b671de7ab7f542458cc575cab1fdb`; queue locking and the PS5 draw-thread adjustment are the only achievement-rendering adaptations retained around that UI.
- The ATRAC9 encoder source is restored from that same pinned Prospero commit. The candidate retains the requested newest-editable-source ordering, its recipe stamp (`version=8;gain=0dB`), and verbatim copy of the pre-encoded fallback.
- `make audio-test` passed: synthetic WAV-to-AT9 RMS ratio was `0.993701` (near `1.0`, with upstream fades and peak-safety ceiling); fixture integrations proved editable WAV takes precedence and the preserved 15-second source is copied byte-for-byte. Both `sce_sys/snd0.source.at9` and the restored `sce_sys/snd0.at9` are SHA-256 `b43c5704d19672ee3a86ed4cba8a627e3fd010b9813f77820e97f9071d449e8a`. The 48 kHz stereo loop passed asset validation.
- The system-notification source, startup queue call, render update call, payload helper, and `notification-test` Makefile target have been removed from the project.
- `bash -n tools/build.sh tools/prepare-assets.sh tools/tests/test_at9_gain.sh` is part of the host validation workflow.
- No PS5 SDK/build or console runtime is available in this workspace. The overlay's on-console appearance and input/gameplay behavior remain unverified. Earlier logs showed missing `/user/app/PPSA99902/sce_sys/pic0.dds` and an invalid `snd0.at9` path; local staging checks cannot establish that the console install contains those files or that Home Screen audio plays there.
