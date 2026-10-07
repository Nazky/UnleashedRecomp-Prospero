#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace system_info_overlay
{
struct Section
{
    std::string title;
    std::vector<std::string> rows;
};

struct Style
{
    ImVec2 position{ 14.0f, 14.0f };
    float fontSize = 10.0f;
    float padding = 8.0f;
    float lineHeight = 14.0f;
    std::string title;
    float titleFontSize = 12.0f;
    float titleLineHeight = 16.0f;
    float titleGap = 5.0f;
    float sectionFontSize = 8.0f;
    float sectionLineHeight = 11.0f;
    float sectionHeaderGap = 2.0f;
    float sectionGap = 6.0f;
    float rowGap = 1.0f;
    ImU32 titleColor = IM_COL32(255, 255, 255, 255);
    ImU32 accentTextColor = IM_COL32(0, 120, 247, 255);
    ImU32 sectionTitleColor = IM_COL32(0, 120, 247, 255);
    bool accentFirstLine = false;
    float accentWidth = 3.0f;
    float sectionAccentWidth = 2.0f;
    float borderThickness = 1.0f;
    float shadowOffset = 3.0f;
    float cornerRadius = 7.0f;
    ImU32 backgroundColor = IM_COL32(9, 13, 20, 226);
    ImU32 borderColor = IM_COL32(255, 255, 255, 35);
    ImU32 separatorColor = IM_COL32(255, 255, 255, 32);
    ImU32 shadowColor = IM_COL32(0, 0, 0, 80);
    ImU32 accentColor = IM_COL32(0, 120, 247, 220);
    ImU32 textColor = IM_COL32(240, 244, 250, 255);
};

// Draws a top-left information card with an optional title and labeled sections.
// The caller owns metric collection, localization, and scaled style values.
void Draw(const std::vector<Section>& sections, ImFont* font, const Style& style);
} // namespace system_info_overlay
