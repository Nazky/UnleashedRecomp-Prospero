#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace system_info_palette
{
struct Color
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

inline constexpr std::size_t kColorCount = 256;
inline constexpr std::size_t kColumns = 6;
inline constexpr std::size_t kRows = 4;
inline constexpr std::size_t kPageSize = kColumns * kRows;
inline constexpr std::size_t kPageCount = (kColorCount + kPageSize - 1) / kPageSize;

constexpr std::array<Color, kColorCount> MakeColors()
{
    std::array<Color, kColorCount> colors{};
    std::size_t index = 0;

    // Keep the existing default Sonic-blue accent as the first and default swatch.
    colors[index++] = { 0, 120, 247 };

    // A complete 6x6x6 web-safe RGB cube.
    constexpr uint8_t levels[] = { 0, 51, 102, 153, 204, 255 };
    for (uint8_t r : levels)
        for (uint8_t g : levels)
            for (uint8_t b : levels)
                colors[index++] = { r, g, b };

    // Forty-step neutral ramp without duplicating the cube's six grays.
    for (int i = 0; i < 39; ++i)
    {
        const auto gray = static_cast<uint8_t>(1 + i * 6);
        colors[index++] = { gray, gray, gray };
    }

    return colors;
}

inline constexpr auto kColors = MakeColors();
static_assert(kColors.size() == kColorCount);

constexpr int32_t ClampIndex(int32_t index)
{
    if (index < 0)
        return 0;
    if (index >= static_cast<int32_t>(kColorCount))
        return static_cast<int32_t>(kColorCount - 1);
    return index;
}

inline int32_t FindNearestIndex(int32_t r, int32_t g, int32_t b)
{
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);

    int32_t nearest = 0;
    int32_t bestDistance = std::numeric_limits<int32_t>::max();
    for (std::size_t i = 0; i < kColors.size(); ++i)
    {
        const int32_t dr = r - kColors[i].r;
        const int32_t dg = g - kColors[i].g;
        const int32_t db = b - kColors[i].b;
        const int32_t distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = static_cast<int32_t>(i);
        }
    }
    return nearest;
}
} // namespace system_info_palette
