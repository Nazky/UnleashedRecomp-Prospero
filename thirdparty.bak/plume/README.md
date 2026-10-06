<div align="center">

# 🪶 plume-ps5

**PlayStation 5 (`__PROSPERO__`) Port of the [plume](https://github.com/renderbag/plume) Rendering Hardware Abstraction Layer**

[![Platform](https://img.shields.io/badge/Platform-PlayStation%205%20(__PROSPERO__)-003791?style=for-the-badge&logo=playstation&logoColor=white)](https://github.com/Nazky/plume-ps5)
[![Branch](https://img.shields.io/badge/Branch-prospero-6e5494?style=for-the-badge&logo=git&logoColor=white)](https://github.com/Nazky/plume-ps5/tree/prospero)
[![Backend](https://img.shields.io/badge/Vulkan-PS5__Mesa%20RADV-AC162C?style=for-the-badge&logo=vulkan&logoColor=white)](https://github.com/mihawk-99/PS5_Mesa)
[![License](https://img.shields.io/badge/License-MIT-2ea44f?style=for-the-badge)](LICENSE)

*Lightweight, high-performance rendering abstraction for Vulkan, Direct3D 12, and Metal — extended on the `prospero` branch with native PlayStation 5 (`x86_64-sie-ps5`) Vulkan support via static RADV (`libvulkan_radeon.ps5.a`) and `VK_KHR_display`.*

---

[**Overview**](#-overview) •
[**Ecosystem**](#-related-repositories) •
[**PS5 Architecture**](#-how-the-playstation-5-port-works) •
[**Quick Start & Build**](#-building--integrating-plume-ps5) •
[**Credits**](#-credits--acknowledgments)

</div>

---

## ✨ Overview

**[plume](https://github.com/renderbag/plume)** is a clean, lightweight, multi-backend rendering abstraction layer originally authored by [Darío Samo (@DarioSamo)](https://github.com/DarioSamo) at [renderbag](https://github.com/renderbag/plume) for **Vulkan**, **Direct3D 12**, and **Metal**.

**[Nazky/plume-ps5](https://github.com/Nazky/plume-ps5)** (`prospero` branch) is a PlayStation 5 fork of `plume` that adds native **`__PROSPERO__` (`x86_64-sie-ps5`)** support to the Vulkan backend (`plume_vulkan.h` / `plume_vulkan.cpp`). All PS5-specific logic is cleanly guarded behind `#if defined(__PROSPERO__)`, allowing cross-platform engines and static recompilation projects to target PlayStation 5 hardware via [PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa)'s static RADV driver (`libvulkan_radeon.ps5.a`) while maintaining 100% compatibility with Windows, Linux, macOS, iOS, and Android.

---

## 🔗 Related Repositories

| Repository | Branch | Description |
| :--- | :--- | :--- |
| **[Nazky/plume-ps5](https://github.com/Nazky/plume-ps5)** | `prospero` | Standalone `plume` library fork with native PlayStation 5 (`__PROSPERO__`) Vulkan backend support. |
| **[Nazky/ps5-plume-triangle](https://github.com/Nazky/ps5-plume-triangle)** | `main` | Self-contained PlayStation 5 Proof-of-Concept app (`PPSA99901`) rendering a 6-DoF DualSense-controlled 3D triangle with a real-time 5x7 bitmap FPS HUD at 4K (`3840x2160`). |
| **[Nazky/UnleashedRecomp-Prospero](https://github.com/Nazky/UnleashedRecomp-Prospero)** | `prospero` | Native PlayStation 5 (`PPSA99902`) port of *Sonic Unleashed Recompiled* powered by `plume-ps5` and `PS5_Mesa`. |

---

## 🛠️ How the PlayStation 5 Port Works

Porting `plume`'s Vulkan backend to the PlayStation 5 (`-target x86_64-sie-ps5`, where Clang defines `__PROSPERO__`) addresses four key architectural differences between desktop Vulkan environments and the PS5 homebrew BigApp runtime:

### 1. Static RADV ICD Dispatch via `volkInitializeCustom`

On desktop platforms, `plume` initializes [volk](https://github.com/zeux/volk) via `volkInitialize()`, which dynamically loads `libvulkan.so.1` / `vulkan-1.dll` at runtime using `dlopen()` / `LoadLibrary()` to resolve `vkGetInstanceProcAddr`.

Inside the PlayStation 5 BigApp sandbox, there is no system Vulkan loader `.sprx`, and loading unsigned dynamic shared libraries is restricted. Instead, the Mesa RADV driver (`libvulkan_radeon.ps5.a`, built with `-Dradv-winsys=ps5`) is linked **statically** into the application's `eboot.bin` (`--whole-archive libvulkan_radeon.ps5.a`), exporting `vk_icdGetInstanceProcAddr`.

Under `#if defined(__PROSPERO__)` in `plume_vulkan.cpp`:
- `vk_icdGetInstanceProcAddr` is declared as a weak external symbol so `plume` compiles cleanly as a static archive and binds to RADV at final link time.
- `VulkanInterface` initializes `volk` via `volkInitializeCustom(ps5PlumeGetInstanceProcAddr)` instead of `volkInitialize()`.
- `ps5PlumeGetInstanceProcAddr()` forwards symbol resolution to `vk_icdGetInstanceProcAddr(instance, pName)` and transparently aliases Vulkan 1.1 core queries (`vkGetPhysicalDeviceProperties2` and `vkGetPhysicalDeviceFeatures2`) to their KHR equivalents (`vkGetPhysicalDeviceProperties2KHR` and `vkGetPhysicalDeviceFeatures2KHR`) when queried prior to instance dispatch table creation:

```cpp
#if defined(__PROSPERO__)
extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance, const char *pName) __attribute__((weak));

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
ps5PlumeGetInstanceProcAddr(VkInstance instance, const char *pName) {
    if (vk_icdGetInstanceProcAddr == nullptr || pName == nullptr) {
        return nullptr;
    }
    PFN_vkVoidFunction fn = vk_icdGetInstanceProcAddr(instance, pName);
    if (fn != nullptr) {
        return fn;
    }
    if (strcmp(pName, "vkGetPhysicalDeviceProperties2") == 0) {
        return vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
    }
    if (strcmp(pName, "vkGetPhysicalDeviceFeatures2") == 0) {
        return vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR");
    }
    return nullptr;
}
#endif
```

### 2. Direct-to-Display WSI (`VK_KHR_display`)

PlayStation 5 does not run X11 or Wayland, and PacBrew's `libSDL2.a` is built without a PS5 Vulkan WSI backend (`SDL_Vulkan_GetInstanceExtensions` and `SDL_Vulkan_CreateSurface` are unsupported). Instead, `PS5_Mesa`'s PS5 winsys exposes the console's `SceVideoOut` hardware planes directly through the standard Vulkan **`VK_KHR_display`** extension.

Under `#if defined(__PROSPERO__)`:
- `RequiredInstanceExtensions` requests `VK_KHR_SURFACE_EXTENSION_NAME` and `VK_KHR_DISPLAY_EXTENSION_NAME` (moving `VK_EXT_DEBUG_UTILS_EXTENSION_NAME` to `OptionalInstanceExtensions` since no validation layer loader exists on the console).
- `VulkanSwapChain` invokes `createPs5DisplaySurface(instance, physicalDevice, &surface)`:
  1. Queries physical displays via `vkGetPhysicalDeviceDisplayPropertiesKHR` and display planes via `vkGetPhysicalDeviceDisplayPlanePropertiesKHR`.
  2. Enumerates display modes with `vkGetDisplayModePropertiesKHR`, preferring the native 4K (`3840x2160`) visible region (falling back to `modes[0]` if connected to a lower-resolution display).
  3. Selects a compatible display plane and creates the `VkSurfaceKHR` via `vkCreateDisplayPlaneSurfaceKHR` with `VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR` and `VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR`.

### 3. Swapchain Resolution & Image Usage Negotiation

- **`VulkanSwapChain::getWindowSize()`**: On desktop, `plume` queries the OS window client rect (`GetClientRect`, `XGetWindowAttributes`, or `SDL_Vulkan_GetDrawableSize`). On `__PROSPERO__`, `getWindowSize()` queries `vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)` and returns `surfaceCapabilities.currentExtent` (defaulting to `3840x2160`).
- **`VulkanSwapChain::resize()`**: Masks the requested swapchain image usage flags (`VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`) against `surfaceCapabilities.supportedUsageFlags` so `vkCreateSwapchainKHR` never requests unsupported usage bits from the PS5 display winsys.

### 4. Header & CMake Platform Guards

- **`plume_vulkan.h`**: Guards `#define VK_USE_PLATFORM_XLIB_KHR` with `!defined(__PROSPERO__)` so Linux host X11 headers (`X11/Xlib.h`) are never pulled in when cross-compiling for PS5.
- **`CMakeLists.txt`**: Skips X11 dependency lookup (`find_package(X11)`) when `CMAKE_SYSTEM_NAME STREQUAL "Prospero"` or `__PROSPERO__` is defined.

---

## 📂 Modified Files Summary

| File | Summary of PlayStation 5 (`__PROSPERO__`) Changes |
| :--- | :--- |
| `plume_vulkan.h` | Guard `VK_USE_PLATFORM_XLIB_KHR` with `!defined(__PROSPERO__)`. |
| `plume_vulkan.cpp` | Add `ps5PlumeGetInstanceProcAddr()` (`volkInitializeCustom`), `createPs5DisplaySurface()` (`VK_KHR_display`), PS5 instance extension selection, `VulkanSwapChain::getWindowSize()` via `VkSurfaceCapabilitiesKHR`, and swapchain `supportedUsageFlags` masking. |
| `CMakeLists.txt` | Skip X11 dependency lookup when targeting PlayStation 5 (`__PROSPERO__`). |
| `Makefile` | Standalone PS5 static library build (`build/libplume_ps5.a`) with automatic `PS5_PayloadSDK` discovery/bootstrapping. |

---

## 🚀 Building & Integrating `plume-ps5`

### Cloning the `prospero` Branch

```bash
git clone -b prospero --recursive https://github.com/Nazky/plume-ps5.git
cd plume-ps5
```

### Option 1: Build with `make` (Recommended)

From inside `plume-ps5/`, run:

```bash
make
```

`make` automatically locates `PS5_PayloadSDK` (checking `$PS5_PAYLOAD_SDK`, `../ps5-plume-triangle/.deps/native/ps5-payload-sdk`, `../UnleashedRecomp-Prospero/.deps/native/ps5-payload-sdk`, or bootstrapping it into `./.deps/native/ps5-payload-sdk` if not yet installed) and builds:
- `build/plume_vulkan.o` (`plume_vulkan.o` symlinked at repository root)
- `build/volk.o`
- `build/spirv_reflect.o`
- `build/libplume_ps5.a`

### Option 2: Direct `clang++` Invocation

To compile `plume_vulkan.cpp` directly with Clang targeting `x86_64-sie-ps5` (using `-femulated-tls`, placing `c++/v1` before `target/include`, and `-std=c++17` to avoid FreeBSD `math.h` C11 `_Generic` macro collisions with `<compare>`):

```bash
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$(realpath ./.deps/native/ps5-payload-sdk)}"

clang++ --target=x86_64-sie-ps5 -std=c++17 -O2 -DNDEBUG -fPIC -fno-plt -fno-stack-protector \
    -fvisibility-nodllstorageclass=default -fdenormal-fp-math=ieee \
    -msse4.1 -mssse3 -mcx16 -femulated-tls \
    -isysroot "$PS5_PAYLOAD_SDK" \
    -isystem "$PS5_PAYLOAD_SDK/target/include/c++/v1" \
    -isystem "$PS5_PAYLOAD_SDK/target/include" \
    -I. \
    -Icontrib/Vulkan-Headers/include \
    -Icontrib/VulkanMemoryAllocator/include \
    -Icontrib/volk \
    -Icontrib/SPIRV-Reflect \
    -c plume_vulkan.cpp -o plume_vulkan.o
```

Then link your executable against `PS5_Mesa`'s `libvulkan_radeon.ps5.a` (`--whole-archive`) and `PS5_PayloadSDK`'s `libps5platform.a`. Refer to **[Nazky/ps5-plume-triangle](https://github.com/Nazky/ps5-plume-triangle)** or **[Nazky/UnleashedRecomp-Prospero](https://github.com/Nazky/UnleashedRecomp-Prospero)** for complete end-to-end build, link, and FSELF packaging scripts.

---

## 🙏 Credits & Acknowledgments

This project builds upon the work of the following open-source repositories and authors:

- **[renderbag/plume](https://github.com/renderbag/plume)** — [Darío Samo (@DarioSamo)](https://github.com/DarioSamo) and contributors for creating the original `plume` rendering hardware abstraction layer.
- **[mihawk-99/PS5_Mesa](https://github.com/mihawk-99/PS5_Mesa)** — PlayStation 5 port of the Mesa 3D RADV Vulkan driver (`libvulkan_radeon.ps5.a`) and `VK_KHR_display` PS5 winsys backend.
- **[mihawk-99/PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan)** — PlayStation 5 Vulkan SDK integration, RADV static linking methodology, and runtime reference.
- **[ps5-payload-dev/sdk](https://github.com/ps5-payload-dev/sdk)** & **[mihawk-99/PS5_PayloadSDK](https://github.com/mihawk-99/PS5_PayloadSDK)** — [John Törnblom (@john-tornblom)](https://github.com/john-tornblom), [@mihawk-99](https://github.com/mihawk-99), and contributors for the PlayStation 5 Payload SDK, headers, C/C++ runtime, and `libps5platform`.
- **[KhronosGroup/Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)** — Official Vulkan API headers (`contrib/Vulkan-Headers`).
- **[zeux/volk](https://github.com/zeux/volk)** — [Arseny Kapoulkine (@zeux)](https://github.com/zeux) for the `volk` Vulkan meta-loader (`contrib/volk`).
- **[GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)** — AMD GPUOpen for the Vulkan Memory Allocator library (`contrib/VulkanMemoryAllocator`).
- **[KhronosGroup/SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect)** — Khronos Group for the SPIR-V shader reflection library (`contrib/SPIRV-Reflect`).
- **[Nazky](https://github.com/Nazky)** — PlayStation 5 (`__PROSPERO__`) port and maintenance (`prospero` branch).

---

## 📄 License

`plume` is distributed under the [MIT License](LICENSE).
