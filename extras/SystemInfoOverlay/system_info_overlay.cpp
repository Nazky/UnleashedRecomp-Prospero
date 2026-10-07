#include "system_info_overlay.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>

namespace system_info_overlay
{
void Draw(const std::vector<Section>& sections, ImFont* font, const Style& style)
{
    if ((sections.empty() && style.title.empty()) || font == nullptr)
        return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (drawList == nullptr)
        return;

    float contentWidth = 0.0f;
    if (!style.title.empty())
    {
        contentWidth = font->CalcTextSizeA(
            style.titleFontSize, FLT_MAX, 0.0f, style.title.c_str()).x;
    }

    float contentHeight = 0.0f;
    bool hasVisibleSection = false;
    for (const Section& section : sections)
    {
        if (section.rows.empty())
            continue;

        if (hasVisibleSection)
            contentHeight += style.sectionGap;

        if (!section.title.empty())
        {
            const float headerWidth = font->CalcTextSizeA(
                style.sectionFontSize, FLT_MAX, 0.0f, section.title.c_str()).x;
            contentWidth = std::max(contentWidth, headerWidth + style.sectionAccentWidth + style.sectionHeaderGap);
            contentHeight += style.sectionLineHeight + style.sectionHeaderGap;
        }

        for (const std::string& row : section.rows)
        {
            contentWidth = std::max(contentWidth,
                font->CalcTextSizeA(style.fontSize, FLT_MAX, 0.0f, row.c_str()).x);
        }

        contentHeight += style.lineHeight * float(section.rows.size());
        if (section.rows.size() > 1)
            contentHeight += style.rowGap * float(section.rows.size() - 1);
        hasVisibleSection = true;
    }

    if (!style.title.empty() && hasVisibleSection)
        contentHeight += style.titleLineHeight + style.titleGap;
    else if (!style.title.empty())
        contentHeight += style.titleLineHeight;

    const ImVec2 min = style.position;
    const ImVec2 max = {
        min.x + contentWidth + style.padding * 2.0f + style.accentWidth,
        min.y + contentHeight + style.padding * 2.0f
    };
    const ImVec2 shadowMin = { min.x + style.shadowOffset, min.y + style.shadowOffset };
    const ImVec2 shadowMax = { max.x + style.shadowOffset, max.y + style.shadowOffset };

    drawList->AddRectFilled(shadowMin, shadowMax, style.shadowColor, style.cornerRadius);
    drawList->AddRectFilled(min, max, style.backgroundColor, style.cornerRadius);
    drawList->AddRect(min, max, style.borderColor, style.cornerRadius, 0, style.borderThickness);
    drawList->AddRectFilled(min,
        { min.x + style.accentWidth, max.y }, style.accentColor,
        style.cornerRadius, ImDrawFlags_RoundCornersLeft);

    float textY = min.y + style.padding;
    const float textX = min.x + style.padding + style.accentWidth;
    if (!style.title.empty())
    {
        drawList->AddText(font, style.titleFontSize, { textX, textY },
            style.titleColor, style.title.c_str());
        textY += style.titleLineHeight;
        if (hasVisibleSection)
        {
            const float dividerY = textY + style.titleGap * 0.5f;
            drawList->AddLine(
                { textX, dividerY }, { max.x - style.padding, dividerY },
                style.separatorColor, 1.0f);
            textY += style.titleGap;
        }
    }

    bool firstSectionDrawn = false;
    bool firstMetricDrawn = false;
    for (const Section& section : sections)
    {
        if (section.rows.empty())
            continue;

        if (firstSectionDrawn)
        {
            textY += style.sectionGap * 0.5f;
            drawList->AddLine(
                { textX, textY }, { max.x - style.padding, textY },
                style.separatorColor, 1.0f);
            textY += style.sectionGap * 0.5f;
        }

        if (!section.title.empty())
        {
            const float markerY = textY + (style.sectionLineHeight - style.sectionAccentWidth) * 0.5f;
            drawList->AddRectFilled(
                { textX, markerY },
                { textX + style.sectionAccentWidth, markerY + style.sectionAccentWidth },
                style.accentColor,
                style.sectionAccentWidth * 0.5f);
            drawList->AddText(font, style.sectionFontSize,
                { textX + style.sectionAccentWidth + style.sectionHeaderGap, textY },
                style.sectionTitleColor, section.title.c_str());
            textY += style.sectionLineHeight + style.sectionHeaderGap;
        }

        for (std::size_t rowIndex = 0; rowIndex < section.rows.size(); ++rowIndex)
        {
            const std::string& row = section.rows[rowIndex];
            const ImU32 rowColor = style.accentFirstLine && !firstMetricDrawn
                ? style.accentTextColor
                : style.textColor;
            drawList->AddText(font, style.fontSize, { textX, textY }, rowColor, row.c_str());
            textY += style.lineHeight;
            firstMetricDrawn = true;
            if (rowIndex + 1 < section.rows.size())
                textY += style.rowGap;
        }
        firstSectionDrawn = true;
    }
}
} // namespace system_info_overlay
