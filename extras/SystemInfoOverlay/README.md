# Reusable ImGui System-Information Overlay

This small renderer draws a compact, sectioned information card. The host game supplies its own metrics, section labels, and localization; the renderer has no dependency on Sonic Unleashed or PS5 APIs.

## Files

- `system_info_overlay.h`
- `system_info_overlay.cpp`

## Requirements

- C++17 or newer
- Dear ImGui headers and library
- A current ImGui context and frame when `Draw()` is called

The renderer is distributed under this project's GPL-3.0 license; `COPYING` is included with the portable bundle.

## Integrate

1. Copy the two source files into your project and add `system_info_overlay.cpp` to its build.
2. Ensure the ImGui include directory is available.
3. Include `system_info_overlay.h` and call `system_info_overlay::Draw()` after `ImGui::NewFrame()` and before `ImGui::Render()`.
4. Collect metrics, group them into `Section` objects, and provide already-localized section headings and row strings.
5. Scale the style values for your target resolution/UI scale. Set `Style::title` for the game-name heading. `Style::accentColor` sets the accent stripe; `Style::sectionTitleColor` colors the section headings; `Style::accentFirstLine` highlights the first metric row.

Example:

```cpp
std::vector<system_info_overlay::Section> sections{
    { "PERFORMANCE", { "FPS: 120.0", "GPU frame: 4.20 ms" } },
    { "MEMORY", { "Game RAM heap: 120.0/256.0 MiB" } },
    { "PATHS", { "User data: /data/MyGame" } }
};

system_info_overlay::Style style;
style.position = { 14.0f, 14.0f };
style.fontSize = 10.0f;
style.padding = 8.0f;
style.lineHeight = 14.0f;
style.sectionFontSize = 8.0f;
style.title = "MY GAME";
style.titleFontSize = 13.0f;
style.accentColor = IM_COL32(0, 120, 247, 235);
style.titleColor = IM_COL32(0, 120, 247, 255);
style.sectionTitleColor = IM_COL32(0, 120, 247, 255);
style.accentFirstLine = true;
system_info_overlay::Draw(sections, myImGuiFont, style);
```

Simple mode can pass one section with an empty title to show only the title and a single FPS row. The renderer handles panel sizing, separators, section labels, border, shadow, and accent styling. Metrics gathering, caching, configuration, and translations remain with the host game.

The PS5 integration example is in `UnleashedRecomp/gpu/video.cpp`. It reads the title from `sce_sys/param.json`, separates performance, SoC-sensor, memory, display, and path rows, and deliberately omits CPU/GPU temperature readings.
