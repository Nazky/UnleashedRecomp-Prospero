<div align="center">

<img src="https://raw.githubusercontent.com/hedge-dev/UnleashedRecompResources/refs/heads/main/images/logo/Logo.png" width="512" alt="Unleashed Recompiled Logo" />

# UnleashedRecomp-Prospero

**Native PlayStation 5 Port of [Sonic Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp)**

[![Platform](https://img.shields.io/badge/Platform-PlayStation%205-003791?style=for-the-badge&logo=playstation&logoColor=white)](https://github.com/Nazky/UnleashedRecomp-Prospero)
[![Branch](https://img.shields.io/badge/Branch-prospero-6e5494?style=for-the-badge&logo=git&logoColor=white)](https://github.com/Nazky/UnleashedRecomp-Prospero/tree/prospero)
[![Title ID](https://img.shields.io/badge/Title%20ID-PPSA99902-0070D1?style=for-the-badge)](https://github.com/Nazky/UnleashedRecomp-Prospero)
[![Renderer](https://img.shields.io/badge/Vulkan-plume--ps5%20%2B%20RADV-AC162C?style=for-the-badge&logo=vulkan&logoColor=white)](https://github.com/Nazky/plume-ps5/tree/prospero)
[![License](https://img.shields.io/badge/License-GPL--3.0-2ea44f?style=for-the-badge)](LICENSE.md)

[**Overview**](#-overview) •
[**PS5 Features**](#-playstation-5-features--enhancements) •
[**Known Issues**](#️-known-issues) •
[**TODO / Roadmap**](#-roadmap--todo) •
[**Build & Package**](#-building-from-source-linux-x86_64) •
[**Deploy to PS5**](#-deploying--running-on-playstation-5) •
[**Custom Assets**](#-customizing-ps5-home-screen-assets--audio) •
[**Architecture**](#-engineering--architecture-deep-dive) •
[**Credits**](#-credits--acknowledgments)

</div>

---

> [!WARNING]
> **Unofficial PlayStation 5 Fork — Not Intended for Upstream Merge**
>
> **UnleashedRecomp-Prospero** ([`Nazky/UnleashedRecomp-Prospero`](https://github.com/Nazky/UnleashedRecomp-Prospero), branch `prospero`) is an independent, community-maintained fork created specifically to port *Unleashed Recompiled* to the **PlayStation 5 (`__PROSPERO__`)**.
>
> - **This repository is NOT meant to be merged into the official [hedge-dev/UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) project.**
> - **Do NOT report issues, crashes, or support requests from this PlayStation 5 fork to the official `hedge-dev/UnleashedRecomp` repository.**
> - **No copyrighted game assets, executables, or Xbox 360 files are included in this repository.** You must provide your own legally acquired Xbox 360 copy of *Sonic Unleashed* (US or EU retail release + Title Update) to recompile and build this project.

---

## ⚡ Overview

**UnleashedRecomp-Prospero** brings the static Xbox 360 recompilation of *Sonic Unleashed* natively to the **PlayStation 5 (`x86_64-sie-ps5` / `__PROSPERO__`)** as a self-contained homebrew BigApp title (`PPSA99902`).

By combining [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) (PowerPC-to-C++ static recompilation) and [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) (Xbox 360 Xenos shader-to-SPIR-V transpilation) with **[plume-ps5](https://github.com/Nazky/plume-ps5)** (`prospero` branch), **[PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa)** (`libvulkan_radeon.ps5.a`), and **[PS5_PayloadSDK](https://github.com/ps5-payload-dev/sdk)**, the entire game engine runs natively on the PS5's AMD Zen 2 CPU and Oberon RDNA 2 GPU at up to **4K (`3840x2160`)** and high frame rates.

---

## 🎮 PlayStation 5 Features & Enhancements

| Feature | Description |
| :--- | :--- |
| **Native 4K Vulkan Rendering** | Powered by [`Nazky/plume-ps5`](https://github.com/Nazky/plume-ps5) (`VK_KHR_display`) and [`PS5_Mesa`](https://github.com/mihawk-99/PS5_Mesa)'s static RADV driver (`libvulkan_radeon.ps5.a`), outputting directly to PS5 `SceVideoOut` at `3840x2160`. |
| **Zero-Wizard `ressources/` Auto-Detection** | Place your extracted Xbox 360 `game`, `update`, and optional `dlc` folders inside `ressources/` — `make` automatically recompiles the XEX & shaders, and the PS5 runtime bypasses the desktop Install Wizard on boot. |
| **Auto-Detected PS5 System Language** | Queries `sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_LANG)` on first boot to automatically configure the game in English, Japanese, German, French, Spanish, or Italian. |
| **In-Process Soft Reboot & Language Prompt** | Changing a restart-requiring setting (`Language`, `Voice Language`, or `Channel Configuration`) in the Options menu displays a confirmation popup in the newly selected language on exit: confirming performs an instant **in-process soft reboot** without closing the PS5 app, while cancelling reverts the setting and returns to the menu. |
| **Console-Tailored Options & UI** | Hides desktop-only settings (`Window Size`, `Monitor`, `Music Attenuation`, `Allow Background Input`, and the Title Menu `Install` entry), sets PS5-optimized defaults (`Fullscreen = true`, `ControllerIcons = PlayStation`, `AchievementNotifications = true`), and saves user data to `/app0/user`. |
| **Native DualSense (`scePad`) + Touchpad** | Polls the DualSense controller directly via `scePadReadState` (with automatic `scePadGetHandle` fallback) and supports navigating the World Map globe using the DualSense touchpad. |
| **Zero-Latency 48 kHz `sceAudioOut`** | Streams 256-sample stereo audio frames directly to Sony's `sceAudioOut` hardware API at `48000 Hz`, matching Xbox 360 XAudio's native 256-sample grain without resampling latency. |
| **Built-In Linux PS5 Asset Pipeline** | Includes native Linux converters for **4K `BC7_UNORM` DX10 DDS** backgrounds (`tools/png_to_bc7_dds`) and **looped 48 kHz ATRAC9** audio (`tools/wav_to_at9`) — no Windows tools required. |

---

## ⚠️ Known Issues

- **Game Crash / Freeze During Reboot (Language Change)**: Changing the game language triggers a game reboot; because a PS5 homebrew BigApp cannot self-respawn its process like on desktop, attempting to change the language and reboot in-game can cause the game to crash or freeze, requiring you to close and relaunch the application manually (the new language setting is still saved for the next launch).
- **Temporary FPS Drops in Heavy Scenes**: The game runs at **4K 60 FPS** in most areas, but scenes with heavy particle effects and many NPCs can cause temporary frame rate drops for a few seconds. **Workaround**: Lower graphics settings such as *Resolution Scale*, *Shadow Resolution*, or *Anti-Aliasing* in the Options menu.
- **Input Lag**: A slight amount of controller input latency can be felt on PS5. There is currently no complete fix for this yet.

---

## 🗺️ Roadmap & TODO

- [ ] **Fix In-Game Reboot / Language Switching**: Resolve the crash/freeze when rebooting the game after changing language or restart-requiring settings.
- [ ] **Native PlayStation 5 Trophies**: Replace the built-in *Unleashed Recompiled* achievement system with native PlayStation 5 system trophies.
- [ ] **Additional PlayStation 5 Features**: Explore and integrate more PS5-specific features (DualSense haptics/adaptive triggers, system integration, and further performance/latency optimizations).

---

## 🚀 Building from Source (Linux x86_64)

### 1. Host Prerequisites

Works out of the box on modern x86_64 Linux distributions (**Ubuntu / Debian**, **Fedora**, **Arch Linux**, etc.):

- **Compilers & Toolchain**: `clang` & `lld` (`18+`), `gcc` & `g++`, `llvm` (`llvm-ar`, `llvm-nm`, `llvm-objcopy`), `cmake`, `meson`, `ninja`, `pkg-config`, `make`
- **Libraries & Utilities**: `sdl2-devel` / `libsdl2-dev`, `python3` (`python3-mako`, `python3-yaml`), `git`, `curl`, `xz`, `tar`, `rsync`

### 2. Clone the `prospero` Branch

```bash
git clone -b prospero https://github.com/Nazky/UnleashedRecomp-Prospero.git
cd UnleashedRecomp-Prospero
```

### 3. Populate `ressources/` with Your Retail Xbox 360 Files

Place your extracted, unmodified Xbox 360 *Sonic Unleashed* (US or EU) files into the `ressources/` directory:

```text
UnleashedRecomp-Prospero/
└── ressources/
    ├── game/       # Extracted Xbox 360 base game files (must include default.xex, shader.ar or #shader.ar.00, etc.)
    ├── update/     # Extracted Xbox 360 Title Update files (must include default.xexp, etc.)
    └── dlc/        # (Optional) Extracted Xbox 360 Adventure Pack DLC folders
```

### 4. Run `make`

```bash
make
```

Running `make` executes the entire end-to-end PlayStation 5 build pipeline automatically:

1. **PS5 `sce_sys/` Asset Preparation (`tools/prepare-assets.sh`)**:
   - Compiles the native Linux `png_to_bc7_dds` and `wav_to_at9` converters and validates/generates `sce_sys/icon0.png`, `sce_sys/pic0.dds`, `sce_sys/pic1.dds`, and `sce_sys/snd0.at9`.
2. **Host Recompiler Tools (`tools/build-host-tools.sh`)**:
   - Compiles native Linux host binaries for `XenonRecomp`, `XenosRecomp` (with `-DUNLEASHED_RECOMP`), `x_decompress`, `file_to_c`, and `ps5-native-tool` into `tools/bin/`.
3. **Xbox 360 XEX & Xenos Shader Recompilation (`tools/recomp-xex.sh`)**:
   - Patches `default.xex` + `default.xexp` into `ressources/patched/default.xex` and generates **261 PowerPC-to-C++ source files** (`UnleashedRecompLib/ppc/ppc_recomp.0.cpp` .. `ppc_recomp.260.cpp`).
   - Decompresses `shader.ar` via `x_decompress` and runs `XenosRecomp` to transpile all Xbox 360 Xenos shaders into SPIR-V/DXIL C++ tables (`UnleashedRecompLib/shader/shader_cache.cpp`).
4. **Embedded UI & Shader Resources (`tools/generate-resources.sh`)**:
   - Compresses and embeds UI textures, fonts, and shader caches into C++ headers via `file_to_c` (`zstd`).
5. **PS5 SDK, PacBrew & Static RADV Driver (`tools/setup-native-dependencies.sh`, `tools/setup-pacbrew-dependencies.sh`, `tools/build-radv.sh`)**:
   - Bootstraps `PS5_PayloadSDK` (`libc.a`, `libps5platform.a`, `libc++.a`, `libc++abi.a`, `libunwind.a`) and PacBrew static libraries (`libSDL2.a`, `libzstd.a`, `libfreetype.a`, `libiconv.a`).
   - Builds `PS5_Mesa`'s `libvulkan_radeon.ps5.a` (`-Dradv-winsys=ps5`) using the pre-generated OpenCL SPIR-V cache in `tooling/radv/clc-cache.tar.xz`.
6. **Cross-Compile, Link & FSELF Sign (`tools/build.sh` + `tools/radv-link.sh`)**:
   - Cross-compiles all C/C++ sources for `x86_64-sie-ps5` (`-msse4.1 -mssse3 -mcx16 -femulated-tls`), links with `libvulkan_radeon.ps5.a` and `ps5-emutls-cxa.o`, and signs the final PS5 FSELF executable (`pkg/PPSA99902/eboot.bin`).

---

## 📦 Deploying & Running on PlayStation 5

### Option A: Automated FTP + Payload Launch

```bash
make deploy PS5_HOST=<PS5_IP_ADDRESS>
```

### Option B: Manual Copy to `/data/homebrew/PPSA99902`

Copy the generated package from `pkg/PPSA99902/` together with your `ressources/` directory to `/data/homebrew/PPSA99902/` on your PlayStation 5 (mounted at `/app0` inside the PS5 BigApp sandbox):

```text
/data/homebrew/PPSA99902/
├── eboot.bin                # Signed PS5 FSELF executable (from pkg/PPSA99902/eboot.bin)
├── sce_module/
│   └── libc.prx             # Signed clean-room runtime module (from pkg/PPSA99902/sce_module/libc.prx)
├── sce_sys/
│   ├── param.json           # Title metadata for PPSA99902
│   ├── icon0.png            # 512x512 PS5 launcher icon
│   ├── pic0.dds             # 3840x2160 BC7_UNORM DX10 DDS Home Screen background
│   ├── pic1.dds             # 3840x2160 BC7_UNORM DX10 DDS launch splash screen
│   └── snd0.at9             # Looped 48 kHz stereo ATRAC9 Home Screen background music
└── ressources/              # Your Xbox 360 game assets + generated patched XEX
    ├── game/                # Retail base game files
    ├── update/              # Retail Title Update files
    ├── patched/             # Generated default.xex (created by make in ressources/patched/default.xex)
    └── dlc/                 # (Optional) Adventure Pack DLC folders
```

Launch `PPSA99902` from your PS5 homebrew launcher or send the launch controller payload to port `9021`:

```bash
nc -N <PS5_IP_ADDRESS> 9021 < pkg/controllers/ps5-UnleashedRecomp-launch-PPSA99902.elf
```

Save data (`save/`), achievements (`achievements.bin`), and configuration (`config.toml`) are stored persistently inside `/data/homebrew/PPSA99902/user/` (`/app0/user/` in the sandbox).

---

## 🎨 Customizing PS5 Home Screen Assets & Audio

`sce_sys/` includes custom *Sonic Unleashed* PS5 presentation assets and native Linux conversion tools (`tools/png_to_bc7_dds` and `tools/wav_to_at9`):

| File | Role | PS5 Format Specification |
| :--- | :--- | :--- |
| `sce_sys/icon0.png` | PS5 Home Screen tile icon | `512x512` 32-bit RGBA PNG |
| `sce_sys/pic0.png` → `sce_sys/pic0.dds` | Home Screen selection background | `3840x2160` `BC7_UNORM` (`DXGI_FORMAT_BC7_UNORM = 98`) DX10 2D DDS, 1 mip (`8,294,548` B) |
| `sce_sys/pic1.png` → `sce_sys/pic1.dds` | Game launch splash screen | `3840x2160` `BC7_UNORM` (`DXGI_FORMAT_BC7_UNORM = 98`) DX10 2D DDS, 1 mip (`8,294,548` B) |
| `sce_sys/snd0.{wav,mp3,ogg,flac}` → `sce_sys/snd0.at9` | Home Screen background music | Looped `48000` Hz stereo ATRAC9 RIFF/WAVE (`<= 2 MiB`, up to ~87s) |

To replace any asset via the CLI helper:

```bash
bash tools/prepare-assets.sh \
    --icon /path/to/icon.png \
    --selection-background /path/to/home-bg.png \
    --launch-background /path/to/splash-bg.png \
    --audio /path/to/music.mp3
```

Or simply replace `sce_sys/pic0.png`, `sce_sys/pic1.png`, `sce_sys/icon0.png`, or `sce_sys/snd0.mp3` and run `make assets`.

---

## 🔬 Engineering & Architecture Deep-Dive

### 1. 4 GiB Xbox 360 Guest Memory Arena in PS5 Direct Memory (`UnleashedRecomp/kernel/memory.cpp`)

Recompiled Xbox 360 PowerPC code translates 32-bit guest addresses (`0x00000000..0xFFFFFFFF`) by adding them to `g_memory.base`, requiring a contiguous **4 GiB (`0x100000000` bytes)** virtual address reservation. On PS5, anonymous `mmap` (`MAP_ANONYMOUS`) draws from a ~400 MiB flexible memory pool and cannot satisfy a 4 GiB mapping. Instead, `Memory::Memory()` reserves a 4 GiB virtual range at `0x1000000000`, allocates 4 GiB of physical memory from the PS5 BigApp's **12 GiB Direct Memory (DMEM) pool** via `sceKernelAllocateDirectMemory`, maps it with `sceKernelMapDirectMemory(..., MAP_FIXED, ...)`, and protects the null-pointer guard page (`0x1000000000..0x1000004000`) with `PROT_NONE`.

### 2. Coordinated Emulated TLS & C++ `thread_local` Destructor Runtime (`tools/radv-link.sh`)

PS5 Clang (`-target x86_64-sie-ps5`) uses emulated TLS (`__emutls_get_address`), which allocates per-thread `thread_local` objects under a pthread key (`emutls_pthread_key`) created before `libc++abi`'s `__cxa_thread_atexit_impl` destructor key (`destructors_key`). Without coordination, FreeBSD `libkernel.sprx` (`_thread_cleanupspecific`) frees `emutls` storage into `dlmalloc` *before* running C++ `thread_local` destructors (`std::vector::~vector()`), corrupting `this->__begin_` with `dlmalloc` `smallbin` freelist pointers and causing a `SIGSEGV` on worker thread exit. `tools/radv-link.sh` compiles a unified `ps5-emutls-cxa.o` runtime that drains all C++ `thread_local` destructors while `emutls` buffers are still live and defers `emutls` deallocation (`EMUTLS_SKIP_DESTRUCTOR_ROUNDS = 2`).

### 3. 2 MiB Direct Memory Stacks for Guest PowerPC Threads (`UnleashedRecomp/cpu/guest_thread.h`)

`libkernel.sprx` defaults to 64 KiB thread stacks, which deeply nested PowerPC call trees overflow. On `__PROSPERO__`, `GuestThreadHandle` uses `std::thread`, routing through `libps5platform`'s `--wrap=pthread_create` (`threads.c`) to allocate a **2 MiB stack in PS5 Direct Memory with an unmapped guard page** for every Xbox 360 guest thread.

### 4. Live Language Virtual Filesystem & In-Process Soft Reboot (`kernel/io/file_system.cpp`, `patches/CGameModeStageTitle_patches.cpp`)

To allow switching text and voice languages on PS5 without terminating the BigApp container:
- `XCreateFileA` (`kernel/io/file_system.cpp`) dynamically redirects per-language archive paths (`Languages/<Lang>/*.ar.00`, `*.arl`) and voice archive paths (`voices/<Lang>/*`) to match the active `Config::Language` and `Config::VoiceLanguage`.
- Confirming the localized restart prompt when leaving the Options menu sets `App::s_isSoftRebootRequested = true`, which `SWA::CGameModeStageTitle::Update` (`sub_825518B8`) handles on the main thread by syncing `CApplicationDocument`, updating CriAtom voice bindings (`sub_825198C8`), and transitioning `SWA::CGame` (`sub_825517C8`) to a fresh `CGameModeStageTitle` state that reloads `Title.ar.00` and `WorldMap.ar.00` in the new language.

---

## 🛠️ Makefile Targets Reference

| Command | Description |
| :--- | :--- |
| `make` | Full automatic build: prepares `sce_sys/` assets, builds host tools, recompiles XEX & shaders from `ressources/`, builds RADV & SDK dependencies, compiles PS5 sources, and signs `pkg/PPSA99902/eboot.bin`. |
| `make assets` | Converts `sce_sys/pic0.png` & `sce_sys/pic1.png` to 4K `BC7_UNORM` DX10 DDS (`pic0.dds` & `pic1.dds`), converts `sce_sys/snd0.{wav,mp3,ogg,flac}` to looped 48 kHz ATRAC9 (`snd0.at9`), and validates `sce_sys/`. |
| `make tools` | Builds host recompiler and asset tools (`XenonRecomp`, `XenosRecomp`, `x_decompress`, `file_to_c`, `png_to_bc7_dds`, `wav_to_at9`, `ps5-native-tool`) into `tools/bin/`. |
| `make recomp` | Runs the Xbox 360 XEX + Xenos shader + DLC recompilation pipeline from `ressources/{game,update,dlc}`. |
| `make deploy PS5_HOST=<IP>` | Uploads `dist/PPSA99902/` to `/data/homebrew/PPSA99902/` via FTP and launches the title via port `9021`. |
| `make close PS5_HOST=<IP>` | Sends the close controller payload to terminate `PPSA99902` on the console. |
| `make clean` | Removes intermediate `build/`, `dist/`, and `pkg/` build artifacts. |

---

## 🙏 Credits & Acknowledgments

### Upstream *Unleashed Recompiled* Team & Repositories

All credit for the original *Unleashed Recompiled* project, recompilers, custom UI, artwork, and research goes to the **[hedge-dev](https://github.com/hedge-dev)** team and contributors:

- **[hedge-dev/UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp)** — The official *Sonic Unleashed Recompiled* project:
  - **[Skyth (@blueskythlikesclouds)](https://github.com/blueskythlikesclouds)** — Creator and Lead Developer of *Unleashed Recompiled*, **[XenonRecomp](https://github.com/hedge-dev/XenonRecomp)**, and **[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)**, graphics/audio backends, custom menus, dynamic UI aspect ratio, and game patches.
  - **[Sajid (@Sajidur78)](https://github.com/Sajidur78)** — Co-creator and Developer of *Unleashed Recompiled*, **[XenonAnalyse](https://github.com/hedge-dev/XenonRecomp)**, and the Xbox 360 kernel translation layer.
  - **[Hyper (@hyperbx)](https://github.com/hyperbx)** — Developer of system-level features, achievement system, custom menus, patches, and options menu thumbnails.
  - **[Darío (@DarioSamo)](https://github.com/DarioSamo)** — Creator of **[plume](https://github.com/renderbag/plume)**, shader research, installer wizard, Linux support, and Spanish localization.
  - **[ĐeäTh (@DeaTh-G)](https://github.com/DeaTh-G)** — Game-accurate UI design supervision, Japanese ruby annotation support, and localization support.
  - **[RadiantDerg (@RadiantDerg)](https://github.com/RadiantDerg)** — Lead Artist for options menu thumbnails and game internals research.
  - **[PTKay (@PTKay)](https://github.com/PTKay)** — Lead Concept Artist for custom menus and installer wizard visuals.
  - **[SuperSonic16 (@thesupersonic16)](https://github.com/thesupersonic16)** — Lead Developer of **[Hedge Mod Manager](https://github.com/thesupersonic16/HedgeModManager)** and Linux deployment support.
  - **[NextinHKRY (@NextinMono)](https://github.com/NextinMono)** — Game internals research, thumbnail concept art, and Italian localization.
  - **[LadyLunanova](https://linktr.ee/ladylunanova)** — Achievement trophy sprite and keyboard/mouse icons.
  - **[LJSTAR (@LJSTARbird)](https://github.com/LJSTARbird)** — Project logo artist, options menu thumbnails, button guide icons, and French localization.
  - **[saguinee](https://twitter.com/saguinee)** — Options menu thumbnail artist.
  - **[Goalringmod27](https://linktr.ee/goalringmod27)** — Achievements overlay concept artist and thumbnail assistance.
  - **[RagdollClash (@RagdollClash)](https://github.com/RagdollClash)** — Provisional dynamic UI aspect ratio support.
  - **[DaGuAr](https://twitter.com/TheDaguar)**, **[brianuuuSonic (@brianuuu)](https://github.com/brianuuu)**, and **[Kitzuku (@Kitzuku)](https://github.com/Kitzuku)** — Spanish, Japanese, and German localization.
- **[hedge-dev/XenonRecomp](https://github.com/hedge-dev/XenonRecomp)** — Xbox 360 PowerPC-to-C++ static recompiler and analyser.
- **[hedge-dev/XenosRecomp](https://github.com/hedge-dev/XenosRecomp)** — Xbox 360 Xenos shader-to-HLSL/SPIR-V/DXIL recompiler.
- **[hedge-dev/UnleashedRecompResources](https://github.com/hedge-dev/UnleashedRecompResources)** — Official UI textures, fonts, and image resources for *Unleashed Recompiled*.

### PlayStation 5 Port, Toolchain & Driver Repositories

- **[Nazky](https://github.com/Nazky)** — PlayStation 5 (`__PROSPERO__`) port, runtime adaptations, PS5 QoL features, soft-reboot architecture, and Linux PS5 asset pipeline (**[Nazky/UnleashedRecomp-Prospero](https://github.com/Nazky/UnleashedRecomp-Prospero)**).
- **[Nazky/plume-ps5](https://github.com/Nazky/plume-ps5)** (`prospero` branch) & **[renderbag/plume](https://github.com/renderbag/plume)** — PlayStation 5 (`__PROSPERO__`) Vulkan backend port of the `plume` rendering abstraction layer.
- **[Nazky/ps5-plume-triangle](https://github.com/Nazky/ps5-plume-triangle)** — Companion PS5 4K Vulkan & DualSense reference application (`PPSA99901`).
- **[mihawk-99/PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa)** — PlayStation 5 port of the Mesa 3D RADV Vulkan driver (`libvulkan_radeon.ps5.a`) and `VK_KHR_display` PS5 winsys backend.
- **[mihawk-99/PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan)** & **[mihawk-99/PS5_vkQuake](https://github.com/mihawk-99/PS5_vkQuake)** — [@mihawk-99](https://github.com/mihawk-99) for the PlayStation 5 Vulkan toolchain, RADV static link recipe, clean-room `libc.prx` builder, FSELF signer (`ps5-native-tool`), and controller payloads.
- **[ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk)** & **[mihawk-99/PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK)** — [John Törnblom (@john-tornblom)](https://github.com/john-tornblom), [@mihawk-99](https://github.com/mihawk-99), and contributors for the PlayStation 5 Payload SDK, C/C++ standard libraries, Direct Memory heap/thread allocator (`libps5platform`), and Sony system service bindings.
- **[ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo)** — Prebuilt PlayStation 5 static port libraries (`libSDL2`, `libzstd`, `libfreetype`, `libiconv`, `libpng`, `liblzma`, `zlib`).
- **[earthonion/ps5-at9-converter](https://github.com/earthonion/ps5-at9-converter)** — Reference for PlayStation 5 `snd0.at9` ATRAC9 container specifications.

### Third-Party Libraries & Research Projects

- **[N64Recomp/N64Recomp](https://github.com/N64Recomp/N64Recomp)** ([Mr-Wiseguy](https://github.com/Mr-Wiseguy)) — Static recompilation inspiration and guidance.
- **[xenia-project/xenia](https://github.com/xenia-project/xenia)** — Foundational Xbox 360 kernel, XEX, and Xenos GPU research.
- **[ocornut/imgui](https://github.com/ocornut/imgui)** & **[epezent/implot](https://github.com/epezent/implot)** — Immediate-mode GUI and plotting libraries powering the custom menus and performance overlay.
- **[Thealexbarney/LibAtrac9](https://github.com/Thealexbarney/LibAtrac9)** — ATRAC9 audio encoder/decoder library (`thirdparty/libatrac9`) used by `tools/wav_to_at9`.
- **[mackron/dr_libs](https://github.com/mackron/dr_libs)** (`dr_mp3`, `dr_flac`) & **[nothings/stb](https://github.com/nothings/stb)** (`stb_image`, `stb_image_resize`, `stb_vorbis`) — Single-header audio and image codecs used by the engine and PS5 asset converters.
- **[pavel-kirienko/o1heap](https://github.com/pavel-kirienko/o1heap)** — Constant-complexity deterministic memory allocator used for guest heap management.
- **[cameron314/concurrentqueue](https://github.com/cameron314/concurrentqueue)** — Lock-free multi-producer/multi-consumer queue.
- **[redorav/ddspp](https://github.com/redorav/ddspp)** — Header-only DDS texture parser.
- **[nlohmann/json](https://github.com/nlohmann/json)**, **[Neargye/magic_enum](https://github.com/Neargye/magic_enum)**, **[martinus/unordered_dense](https://github.com/martinus/unordered_dense)**, **[marzer/tomlplusplus](https://github.com/marzer/tomlplusplus)**, **[fmtlib/fmt](https://github.com/fmtlib/fmt)**, and **[Cyan4973/xxHash](https://github.com/Cyan4973/xxHash)** — Core C++ containers, reflection, serialization, formatting, and hashing libraries.
- **[simd-everywhere/simde](https://github.com/simd-everywhere/simde)** — Portable SIMD intrinsics translation layer used by recompiled VMX128 PowerPC code.
- **[kyz/libmspack](https://github.com/kyz/libmspack)**, **[kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c)**, and **[vog/sha1](https://github.com/vog/sha1)** — LZX decompression, AES decryption, and SHA-1 hashing for Xbox 360 XEX/container parsing.
- **[aras-p/smol-v](https://github.com/aras-p/smol-v)** & **[microsoft/DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)** — SPIR-V compression and HLSL-to-SPIR-V/DXIL shader compilation.
- **[KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)**, **[zeux/volk](https://github.com/zeux/volk)**, **[GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)**, and **[KhronosGroup/SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)** — Vulkan headers, meta-loader, memory allocator, and shader reflection.
- **[libsdl-org/SDL](https://github.com/libsdl-org/SDL)** & **[facebook/zstd](https://github.com/facebook/zstd)** — Event/controller abstraction and Zstandard compression.

---

## 📄 License

This project is licensed under the **[GNU General Public License v3.0 (GPL-3.0)](LICENSE.md)**.
