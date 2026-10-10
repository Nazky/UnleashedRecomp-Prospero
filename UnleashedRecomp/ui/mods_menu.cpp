#include "mods_menu.h"

#include <api/SWA.h>
#include <exports.h>
#include <gpu/imgui/imgui_snapshot.h>
#include <hid/hid.h>
#include <locale/locale.h>
#include <patches/aspect_ratio_patches.h>
#include <ui/button_guide.h>
#include <ui/imgui_utils.h>
#include <user/paths.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace
{
    ImFont* g_titleFont{};
    ImFont* g_textFont{};
    ImFont* g_smallFont{};

    std::vector<UserModInfo> g_originalMods;
    std::vector<UserModInfo> g_workingMods;
    size_t g_selectedModIndex{};
    size_t g_selectedOptionIndex{};
    size_t g_firstVisibleModIndex{};
    size_t g_firstVisibleOptionIndex{};
    float g_modScrollPosition{};
    float g_optionScrollPosition{};
    double g_appearTime{};
    double g_modSelectionTime{};
    double g_optionSelectionTime{};
    bool g_isMovingMod{};
    bool g_isEditingOptions{};
    bool g_inputBuffered{};
    std::string g_errorMessage;

    constexpr size_t MAX_VISIBLE_MODS = 9;
    constexpr size_t MAX_VISIBLE_OPTIONS = 6;

    bool HasChanges()
    {
        if (g_workingMods.size() != g_originalMods.size())
            return true;

        for (size_t i = 0; i < g_workingMods.size(); ++i)
        {
            if (g_workingMods[i].folderName != g_originalMods[i].folderName)
                return true;

            const auto& workingMod = g_workingMods[i];
            const auto originalMod = std::find_if(g_originalMods.begin(), g_originalMods.end(), [&](const UserModInfo& mod)
            {
                return mod.folderName == workingMod.folderName;
            });
            if (originalMod == g_originalMods.end())
                return true;

            for (const auto& workingOption : workingMod.options)
            {
                const auto originalOption = std::find_if(originalMod->options.begin(), originalMod->options.end(), [&](const ModConfigOption& option)
                {
                    return option.sectionName == workingOption.sectionName &&
                        option.name == workingOption.name && option.iniFile == workingOption.iniFile;
                });
                if (originalOption == originalMod->options.end() || originalOption->value != workingOption.value)
                    return true;
            }
        }

        return false;
    }

    UserModInfo* GetSelectedMod()
    {
        return g_selectedModIndex < g_workingMods.size() ? &g_workingMods[g_selectedModIndex] : nullptr;
    }

    void UpdateButtonGuide()
    {
        ButtonGuide::Close();

        if (g_isEditingOptions)
        {
            std::array<Button, 2> buttons =
            {
                Button("Mods_Button_Change", 120.0f, EButtonIcon::X, EButtonAlignment::Left),
                Button("Common_Back", 75.0f, EButtonIcon::B, EButtonAlignment::Right)
            };
            ButtonGuide::Open(buttons);
        }
        else
        {
            std::array<Button, 3> buttons =
            {
                Button(g_isMovingMod ? "Mods_Button_Drop" : "Mods_Button_Reorder", 130.0f, EButtonIcon::A, EButtonAlignment::Left),
                Button("Mods_Button_Options", 120.0f, EButtonIcon::X, EButtonAlignment::Left),
                Button("Common_Back", 75.0f, EButtonIcon::B, EButtonAlignment::Right)
            };
            ButtonGuide::Open(buttons);
        }

        ButtonGuide::SetSideMargins(250.0f);
    }

    void PulseSelection()
    {
        g_modSelectionTime = ImGui::GetTime();
        g_optionSelectionTime = g_modSelectionTime;
        Game_PlaySound("sys_worldmap_cursor");
        hid::PulseMenuVibration();
    }

    void CycleSelectedOption(bool backwards = false)
    {
        UserModInfo* mod = GetSelectedMod();
        if (mod == nullptr || g_selectedOptionIndex >= mod->options.size())
            return;

        ModConfigOption& option = mod->options[g_selectedOptionIndex];
        if (option.choices.empty())
            return;

        const auto currentChoice = std::find_if(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& choice)
        {
            return choice.value == option.value;
        });

        const size_t currentIndex = currentChoice == option.choices.end()
            ? 0
            : static_cast<size_t>(std::distance(option.choices.begin(), currentChoice));
        const size_t nextIndex = currentChoice == option.choices.end()
            ? 0
            : backwards
                ? (currentIndex + option.choices.size() - 1) % option.choices.size()
                : (currentIndex + 1) % option.choices.size();
        option.value = option.choices[nextIndex].value;
        g_optionSelectionTime = ImGui::GetTime();
        g_errorMessage.clear();
        Game_PlaySound("sys_worldmap_decide");
        hid::PulseMenuVibration();
    }

    std::string GetChoiceDisplayName(const ModConfigOption& option)
    {
        const auto choice = std::find_if(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& value)
        {
            return value.value == option.value;
        });
        if (choice != option.choices.end())
            return choice->displayName;
        if (option.value.empty())
            return Localise("Mods_Value_Stock");
        return fmt::format("{} {}", Localise("Mods_Value_Custom"), option.value);
    }

    std::string GetSelectedOptionDescription(const ModConfigOption& option)
    {
        const auto choice = std::find_if(option.choices.begin(), option.choices.end(), [&](const ModConfigChoice& value)
        {
            return value.value == option.value;
        });
        if (choice != option.choices.end() && !choice->description.empty())
            return choice->description;
        return option.description;
    }

    void ProcessInput()
    {
        auto* inputState = SWA::CInputState::GetInstance();
        if (inputState == nullptr)
            return;
        const auto& pad = inputState->GetPadState();

        if (g_inputBuffered)
        {
            if (pad.IsDown(SWA::eKeyState_A) || pad.IsDown(SWA::eKeyState_Start) || pad.IsDown(SWA::eKeyState_X))
                return;
            g_inputBuffered = false;
        }

        if (g_isEditingOptions)
        {
            UserModInfo* mod = GetSelectedMod();
            if (pad.IsTapped(SWA::eKeyState_B))
            {
                g_isEditingOptions = false;
                g_selectedOptionIndex = 0;
                g_firstVisibleOptionIndex = 0;
                g_optionScrollPosition = 0.0f;
                g_errorMessage.clear();
                UpdateButtonGuide();
                Game_PlaySound("sys_worldmap_cansel");
                return;
            }

            if (mod == nullptr || mod->options.empty())
                return;

            if (pad.IsTapped(SWA::eKeyState_DpadUp))
            {
                g_selectedOptionIndex = g_selectedOptionIndex == 0 ? mod->options.size() - 1 : g_selectedOptionIndex - 1;
                PulseSelection();
            }
            else if (pad.IsTapped(SWA::eKeyState_DpadDown))
            {
                g_selectedOptionIndex = (g_selectedOptionIndex + 1) % mod->options.size();
                PulseSelection();
            }
            else if (pad.IsTapped(SWA::eKeyState_X) || pad.IsTapped(SWA::eKeyState_DpadRight))
            {
                CycleSelectedOption();
            }
            else if (pad.IsTapped(SWA::eKeyState_DpadLeft))
            {
                CycleSelectedOption(true);
            }

            return;
        }

        if (pad.IsTapped(SWA::eKeyState_B))
        {
            if (g_isMovingMod)
            {
                g_isMovingMod = false;
                UpdateButtonGuide();
                Game_PlaySound("sys_worldmap_cansel");
            }
            else
            {
                Game_PlaySound("sys_worldmap_cansel");
                ModsMenu::Close();
            }
            return;
        }

        if (g_isMovingMod)
        {
            if (pad.IsTapped(SWA::eKeyState_A))
            {
                g_isMovingMod = false;
                UpdateButtonGuide();
                Game_PlaySound("sys_worldmap_decide");
                return;
            }

            if (!g_workingMods.empty() && pad.IsTapped(SWA::eKeyState_DpadUp) && g_selectedModIndex > 0)
            {
                std::swap(g_workingMods[g_selectedModIndex], g_workingMods[g_selectedModIndex - 1]);
                --g_selectedModIndex;
                PulseSelection();
            }
            else if (!g_workingMods.empty() && pad.IsTapped(SWA::eKeyState_DpadDown) && g_selectedModIndex + 1 < g_workingMods.size())
            {
                std::swap(g_workingMods[g_selectedModIndex], g_workingMods[g_selectedModIndex + 1]);
                ++g_selectedModIndex;
                PulseSelection();
            }
            return;
        }

        if (g_workingMods.empty())
            return;

        if (pad.IsTapped(SWA::eKeyState_DpadUp))
        {
            g_selectedModIndex = g_selectedModIndex == 0 ? g_workingMods.size() - 1 : g_selectedModIndex - 1;
            PulseSelection();
        }
        else if (pad.IsTapped(SWA::eKeyState_DpadDown))
        {
            g_selectedModIndex = (g_selectedModIndex + 1) % g_workingMods.size();
            PulseSelection();
        }
        else if (pad.IsTapped(SWA::eKeyState_A))
        {
            g_isMovingMod = true;
            UpdateButtonGuide();
            Game_PlaySound("sys_worldmap_decide");
        }
        else if (pad.IsTapped(SWA::eKeyState_X))
        {
            const UserModInfo* mod = GetSelectedMod();
            if (mod != nullptr && !mod->options.empty())
            {
                g_isEditingOptions = true;
                g_selectedOptionIndex = 0;
                g_firstVisibleOptionIndex = 0;
                g_optionScrollPosition = 0.0f;
                g_optionSelectionTime = ImGui::GetTime();
                g_errorMessage.clear();
                UpdateButtonGuide();
                Game_PlaySound("sys_worldmap_decide");
            }
        }
    }

    float IntroProgress()
    {
        const float motion = static_cast<float>(ComputeMotion(g_appearTime, 0.0, 22.0));
        return Hermite(0.0f, 1.0f, motion);
    }

    ImU32 ApplyAlpha(ImU32 color, float alpha)
    {
        ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
        rgba.w *= std::clamp(alpha, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(rgba);
    }

    size_t VisibleRows(float areaHeight, float rowHeight, size_t maximum)
    {
        if (areaHeight <= 0.0f || rowHeight <= 0.0f || maximum == 0)
            return 1;

        const auto rows = static_cast<size_t>(std::floor(areaHeight / rowHeight));
        return std::clamp<size_t>(rows, 1, maximum);
    }

    float SmoothScroll(float current, float target, size_t visibleRows)
    {
        if (std::abs(target - current) > static_cast<float>(visibleRows))
            return target;

        const float deltaTime = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.1f);
        const float motion = 1.0f - std::exp(-18.0f * deltaTime);
        const float result = Lerp(current, target, motion);
        return std::abs(result - target) < 0.01f ? target : result;
    }

    void DrawOptionsPanelFrame(ImVec2 min, ImVec2 max, float alpha)
    {
        auto drawList = ImGui::GetBackgroundDrawList();
        const float grid = Scale(9.0f);

        SetProceduralOrigin(min);
        drawList->AddRectFilled(min, max, ApplyAlpha(IM_COL32(0, 0, 0, 223), alpha));

        SetShaderModifier(IMGUI_SHADER_MODIFIER_CHECKERBOARD);
        const ImU32 outer = ApplyAlpha(IM_COL32(0, 49, 0, 210), alpha);
        const ImU32 inner = ApplyAlpha(IM_COL32(0, 33, 0, 210), alpha);
        drawList->AddRectFilled(min, { max.x, min.y + grid }, outer);
        drawList->AddRectFilled({ min.x, min.y + grid }, { min.x + grid, max.y - grid }, outer);
        drawList->AddRectFilled({ max.x - grid, min.y + grid }, { max.x, max.y - grid }, outer);
        drawList->AddRectFilled({ min.x, max.y - grid }, max, outer);
        drawList->AddRectFilled({ min.x + grid, min.y + grid }, { max.x - grid, max.y - grid }, inner);
        SetShaderModifier(IMGUI_SHADER_MODIFIER_NONE);

        const ImU32 line = ApplyAlpha(IM_COL32(0, 89, 0, 255), alpha);
        const float lineWidth = Scale(2.0f);
        drawList->AddLine({ min.x + grid, min.y + grid }, { max.x - grid, min.y + grid }, line, lineWidth);
        drawList->AddLine({ min.x + grid, min.y + grid }, { min.x + grid, min.y + grid * 2.0f }, line, lineWidth);
        drawList->AddLine({ max.x - grid, min.y + grid }, { max.x - grid, min.y + grid * 2.0f }, line, lineWidth);
        drawList->AddLine({ min.x + grid, max.y - grid }, { max.x - grid, max.y - grid }, line, lineWidth);
        drawList->AddLine({ min.x + grid, max.y - grid }, { min.x + grid, max.y - grid * 2.0f }, line, lineWidth);
        drawList->AddLine({ max.x - grid, max.y - grid }, { max.x - grid, max.y - grid * 2.0f }, line, lineWidth);
        ResetProceduralOrigin();
    }

    void DrawPanelHeader(ImVec2 min, ImVec2 max, std::string_view text, float alpha)
    {
        auto drawList = ImGui::GetBackgroundDrawList();
        const ImU32 top = ApplyAlpha(IM_COL32(0, 130, 0, 190), alpha);
        const ImU32 bottom = ApplyAlpha(IM_COL32(0, 74, 0, 210), alpha);

        SetShaderModifier(IMGUI_SHADER_MODIFIER_SCANLINE_BUTTON);
        drawList->AddRectFilledMultiColor(min, max, top, top, bottom, bottom);
        SetShaderModifier(IMGUI_SHADER_MODIFIER_NONE);
        drawList->AddRect(min, max, ApplyAlpha(IM_COL32(87, 178, 40, 220), alpha), Scale(2.0f), 0, Scale(1.0f));

        const ImVec2 textMin{ min.x + Scale(18.0f), min.y + Scale(7.0f) };
        const ImVec2 textMax{ max.x - Scale(14.0f), max.y - Scale(4.0f) };
        const std::string label(text);
        SetGradient(textMin, textMax,
            ApplyAlpha(IM_COL32(128, 255, 0, 255), alpha),
            ApplyAlpha(IM_COL32(255, 192, 0, 255), alpha));
        DrawTextWithOutline(g_titleFont, Scale(21.0f), textMin,
            ApplyAlpha(IM_COL32(255, 255, 255, 255), alpha), label.c_str(), Scale(1.5f),
            ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha), IMGUI_SHADER_MODIFIER_CATEGORY_BEVEL);
        ResetGradient();
    }

    void DrawOptionsSelection(ImVec2 min, ImVec2 max, float alpha)
    {
        auto drawList = ImGui::GetBackgroundDrawList();
        const float pulse = 0.84f + 0.16f * static_cast<float>((std::sin(ImGui::GetTime() * 4.5) + 1.0) * 0.5);
        const int edgeAlpha = static_cast<int>(128.0f * alpha * pulse);
        const int fillAlpha = static_cast<int>(196.0f * alpha * pulse);

        SetShaderModifier(IMGUI_SHADER_MODIFIER_SCANLINE_BUTTON);
        drawList->AddRectFilledMultiColor(min, max,
            IM_COL32(0xE2, 0x71, 0x22, edgeAlpha), IM_COL32(0xE2, 0x71, 0x22, edgeAlpha),
            IM_COL32(0x92, 0xFF, 0x31, fillAlpha), IM_COL32(0x92, 0xFF, 0x31, fillAlpha));
        SetShaderModifier(IMGUI_SHADER_MODIFIER_NONE);
        drawList->AddRect(min, max, ApplyAlpha(IM_COL32(175, 255, 115, 190), alpha * pulse), 0.0f, 0, Scale(1.0f));
    }

    void DrawScrollBar(ImDrawList* drawList, ImVec2 min, ImVec2 max, size_t total, size_t visible,
        float firstVisible, float alpha)
    {
        if (total <= visible || max.y <= min.y)
            return;

        drawList->AddRectFilled(min, max, ApplyAlpha(IM_COL32(0, 34, 0, 200), alpha), Scale(3.0f));
        const float trackHeight = max.y - min.y;
        const float thumbHeight = std::min(trackHeight,
            std::max(Scale(24.0f), trackHeight * (static_cast<float>(visible) / static_cast<float>(total))));
        const float maxFirst = static_cast<float>(total - visible);
        const float progress = maxFirst > 0.0f ? std::clamp(firstVisible / maxFirst, 0.0f, 1.0f) : 0.0f;
        const float thumbY = min.y + (trackHeight - thumbHeight) * progress;
        const ImVec2 thumbMin{ min.x, thumbY };
        const ImVec2 thumbMax{ max.x, thumbY + thumbHeight };

        SetShaderModifier(IMGUI_SHADER_MODIFIER_SCANLINE_BUTTON);
        drawList->AddRectFilledMultiColor(thumbMin, thumbMax,
            ApplyAlpha(IM_COL32(28, 190, 0, 255), alpha), ApplyAlpha(IM_COL32(28, 190, 0, 255), alpha),
            ApplyAlpha(IM_COL32(146, 255, 49, 255), alpha), ApplyAlpha(IM_COL32(146, 255, 49, 255), alpha));
        SetShaderModifier(IMGUI_SHADER_MODIFIER_NONE);
        drawList->AddRect(thumbMin, thumbMax, ApplyAlpha(IM_COL32(193, 255, 139, 230), alpha), Scale(3.0f), 0, Scale(1.0f));
    }

    void DrawModList(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha)
    {
        DrawOptionsPanelFrame(min, max, alpha);
        DrawPanelHeader({ min.x + Scale(8.0f), min.y + Scale(7.0f) },
            { max.x - Scale(8.0f), min.y + Scale(48.0f) }, Localise("Mods_Panel_List"), alpha);

        const float rowHeight = Scale(42.0f);
        const float rowsTop = min.y + Scale(58.0f);
        const float rowsBottom = max.y - Scale(22.0f);
        const float textSize = Scale(20.0f);
        const size_t modCount = g_workingMods.size();
        const size_t visibleRows = VisibleRows(rowsBottom - rowsTop, rowHeight, MAX_VISIBLE_MODS);
        if (modCount == 0)
        {
            DrawTextBasic(g_textFont, textSize, { min.x + Scale(22.0f), rowsTop + Scale(10.0f) },
                ApplyAlpha(IM_COL32(210, 225, 210, 255), alpha), Localise("Mods_Empty").c_str());
            return;
        }

        const size_t maxFirstVisible = modCount > visibleRows ? modCount - visibleRows : 0;
        if (g_selectedModIndex < g_firstVisibleModIndex)
            g_firstVisibleModIndex = g_selectedModIndex;
        else if (g_selectedModIndex >= g_firstVisibleModIndex + visibleRows)
            g_firstVisibleModIndex = g_selectedModIndex - visibleRows + 1;
        g_firstVisibleModIndex = std::min(g_firstVisibleModIndex, maxFirstVisible);
        g_modScrollPosition = SmoothScroll(g_modScrollPosition, static_cast<float>(g_firstVisibleModIndex), visibleRows);

        const size_t first = std::min(static_cast<size_t>(std::floor(g_modScrollPosition)), modCount - 1);
        const size_t last = std::min(modCount, first + visibleRows + 1);
        const ImVec2 clipMin{ min.x + Scale(10.0f), rowsTop };
        const ImVec2 clipMax{ max.x - Scale(18.0f), rowsBottom };
        drawList->PushClipRect(clipMin, clipMax, true);
        for (size_t index = first; index < last; ++index)
        {
            const float rowY = rowsTop + (static_cast<float>(index) - g_modScrollPosition) * rowHeight;
            const ImVec2 rowMin{ min.x + Scale(11.0f), rowY };
            const ImVec2 rowMax{ max.x - Scale(17.0f), rowY + rowHeight - Scale(2.0f) };

            drawList->AddRectFilled(rowMin, rowMax,
                ApplyAlpha(IM_COL32(0, 20, 0, 110), alpha), Scale(2.0f));
            if (index == g_selectedModIndex)
                DrawOptionsSelection(rowMin, rowMax, alpha);
            else
                drawList->AddRectFilled({ rowMin.x, rowMin.y + Scale(7.0f) },
                    { rowMin.x + Scale(3.0f), rowMax.y - Scale(7.0f) },
                    ApplyAlpha(IM_COL32(0, 125, 0, 180), alpha));

            const std::string& folderName = g_workingMods[index].folderName;
            const bool showMoving = index == g_selectedModIndex && g_isMovingMod;
            const std::string movingText = showMoving ? Localise("Mods_Moving") : std::string{};
            const ImVec2 movingSize = showMoving
                ? g_smallFont->CalcTextSizeA(Scale(15.0f), FLT_MAX, 0.0f, movingText.c_str())
                : ImVec2{};
            const float labelLeft = rowMin.x + Scale(16.0f);
            const float labelRight = rowMax.x - Scale(16.0f) - (showMoving ? movingSize.x + Scale(28.0f) : 0.0f);
            DrawTextWithMarqueeShadow(
                g_textFont,
                textSize,
                { labelLeft, rowMin.y + Scale(8.0f) },
                { labelLeft, rowMin.y },
                { labelRight, rowMax.y },
                ApplyAlpha(index == g_selectedModIndex ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 220, 205, 255), alpha),
                folderName.c_str(),
                g_modSelectionTime,
                1.2,
                45.0,
                2.0f, 1.0f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));

            if (showMoving)
            {
                DrawTextBasic(g_smallFont, Scale(15.0f),
                    { rowMax.x - movingSize.x - Scale(14.0f), rowMin.y + Scale(11.0f) },
                    ApplyAlpha(IM_COL32(255, 201, 52, 255), alpha), movingText.c_str());
            }
        }
        drawList->PopClipRect();

        DrawScrollBar(drawList,
            { max.x - Scale(11.0f), rowsTop },
            { max.x - Scale(5.0f), rowsBottom },
            modCount, visibleRows, g_modScrollPosition, alpha);
    }

    void DrawModDetails(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha)
    {
        DrawOptionsPanelFrame(min, max, alpha);
        DrawPanelHeader({ min.x + Scale(8.0f), min.y + Scale(7.0f) },
            { max.x - Scale(8.0f), min.y + Scale(48.0f) },
            g_isEditingOptions ? Localise("Mods_Panel_Options") : Localise("Mods_Panel_Details"), alpha);

        const UserModInfo* mod = GetSelectedMod();
        if (mod != nullptr && g_isEditingOptions && !mod->options.empty())
        {
            const std::string optionCount = fmt::format("{}", mod->options.size());
            const float badgeWidth = Scale(42.0f);
            const ImVec2 badgeMin{ max.x - Scale(22.0f) - badgeWidth, min.y + Scale(15.0f) };
            const ImVec2 badgeMax{ badgeMin.x + badgeWidth, badgeMin.y + Scale(27.0f) };
            drawList->AddRectFilled(badgeMin, badgeMax, ApplyAlpha(IM_COL32(0, 45, 0, 210), alpha), Scale(3.0f));
            drawList->AddRect(badgeMin, badgeMax, ApplyAlpha(IM_COL32(128, 255, 0, 210), alpha), Scale(3.0f), 0, Scale(1.0f));
            const ImVec2 countSize = g_smallFont->CalcTextSizeA(Scale(14.0f), FLT_MAX, 0.0f, optionCount.c_str());
            DrawTextBasic(g_smallFont, Scale(14.0f),
                { badgeMin.x + (badgeWidth - countSize.x) * 0.5f, badgeMin.y + Scale(5.0f) },
                ApplyAlpha(IM_COL32(200, 255, 125, 255), alpha), optionCount.c_str());
        }

        if (mod == nullptr)
        {
            DrawTextBasic(g_textFont, Scale(20.0f), { min.x + Scale(22.0f), min.y + Scale(72.0f) },
                ApplyAlpha(IM_COL32(220, 230, 220, 255), alpha), Localise("Mods_SelectPrompt").c_str());
            return;
        }

        const float x = min.x + Scale(22.0f);
        const float right = max.x - Scale(22.0f);
        float y = min.y + Scale(58.0f);
        const float bodyTextSize = Scale(17.0f);
        const float titleTextSize = Scale(23.0f);
        const ImVec2 bodyClipMin{ x, y };
        const ImVec2 bodyClipMax{ right, min.y + Scale(152.0f) };
        drawList->PushClipRect(bodyClipMin, bodyClipMax, true);

        const std::string& title = mod->title.empty() ? mod->folderName : mod->title;
        DrawTextWithMarqueeShadow(g_titleFont, titleTextSize, { x, y }, bodyClipMin, bodyClipMax,
            ApplyAlpha(IM_COL32(255, 208, 74, 255), alpha), title.c_str(), g_modSelectionTime, 1.0, 42.0,
            2.0f, 1.0f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
        y += Scale(32.0f);

        if (!mod->author.empty())
        {
            const std::string authorText = fmt::format("{} {}", Localise("Mods_Author"), mod->author);
            DrawTextWithMarqueeShadow(g_smallFont, Scale(15.0f), { x, y }, bodyClipMin, bodyClipMax,
                ApplyAlpha(IM_COL32(180, 205, 185, 255), alpha), authorText.c_str(), g_modSelectionTime, 1.0, 38.0,
                2.0f, 1.0f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
            y += Scale(22.0f);
        }

        if (!mod->description.empty())
        {
            drawList->PushClipRect({ x, y }, bodyClipMax, true);
            DrawTextWithMarqueeShadow(g_textFont, bodyTextSize, { x, y }, { x, y }, bodyClipMax,
                ApplyAlpha(IM_COL32(220, 230, 220, 255), alpha), mod->description.c_str(), g_modSelectionTime, 1.5, 34.0,
                2.0f, 1.0f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
            drawList->PopClipRect();
        }
        drawList->PopClipRect();

        if (!g_errorMessage.empty())
        {
            const float errorY = min.y + Scale(148.0f);
            drawList->PushClipRect({ x, errorY }, { right, errorY + Scale(38.0f) }, true);
            DrawTextBasic(g_smallFont, Scale(15.0f), { x, errorY },
                ApplyAlpha(IM_COL32(255, 120, 105, 255), alpha), g_errorMessage.c_str());
            drawList->PopClipRect();
        }

        if (mod->options.empty())
        {
            DrawTextBasic(g_textFont, bodyTextSize, { x, min.y + Scale(192.0f) },
                ApplyAlpha(IM_COL32(180, 205, 185, 255), alpha), Localise("Mods_NoOptions").c_str());
            return;
        }

        if (!g_isEditingOptions)
        {
            const std::string countText = fmt::format("{}: {}", Localise("Mods_OptionCount"), mod->options.size());
            DrawTextWithOutline(g_titleFont, Scale(18.0f), { x, min.y + Scale(187.0f) },
                ApplyAlpha(IM_COL32(255, 210, 80, 255), alpha), countText.c_str(), Scale(1.0f),
                ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha), IMGUI_SHADER_MODIFIER_CATEGORY_BEVEL);

            const float listTop = min.y + Scale(218.0f);
            const float rowsBottom = max.y - Scale(20.0f);
            const float rowHeight = Scale(31.0f);
            const size_t maxRows = VisibleRows(rowsBottom - listTop, rowHeight, mod->options.size());
            const size_t showCount = mod->options.size() > maxRows
                ? (maxRows > 1 ? maxRows - 1 : 0)
                : maxRows;
            const ImVec2 clipMin{ x, listTop };
            const ImVec2 clipMax{ right, rowsBottom };
            drawList->PushClipRect(clipMin, clipMax, true);

            for (size_t index = 0; index < showCount; ++index)
            {
                const auto& option = mod->options[index];
                const float rowY = listTop + static_cast<float>(index) * rowHeight;
                const ImVec2 rowMin{ x, rowY };
                const ImVec2 rowMax{ right, rowY + rowHeight - Scale(2.0f) };
                drawList->AddRectFilled(rowMin, rowMax, ApplyAlpha(IM_COL32(0, 27, 0, 135), alpha), Scale(2.0f));
                drawList->AddRectFilled({ rowMin.x, rowMin.y + Scale(5.0f) },
                    { rowMin.x + Scale(3.0f), rowMax.y - Scale(5.0f) }, ApplyAlpha(IM_COL32(70, 185, 0, 210), alpha));
                drawList->AddLine({ rowMin.x + Scale(4.0f), rowMax.y }, { rowMax.x, rowMax.y },
                    ApplyAlpha(IM_COL32(0, 89, 0, 170), alpha), Scale(1.0f));

                const std::string label = option.displayName.empty() ? option.name : option.displayName;
                const std::string valueText = GetChoiceDisplayName(option);
                const float valueWidth = std::min(Scale(178.0f), (right - x) * 0.42f);
                const float valueLeft = right - valueWidth;
                DrawTextWithMarqueeShadow(g_textFont, Scale(16.0f),
                    { rowMin.x + Scale(10.0f), rowMin.y + Scale(6.0f) },
                    { rowMin.x + Scale(8.0f), rowMin.y }, { valueLeft - Scale(8.0f), rowMax.y },
                    ApplyAlpha(IM_COL32(220, 230, 220, 255), alpha), label.c_str(), g_optionSelectionTime, 1.0, 38.0,
                    2.0f, 1.0f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));

                const ImVec2 valueMin{ valueLeft, rowMin.y + Scale(3.0f) };
                const ImVec2 valueMax{ right, rowMax.y - Scale(3.0f) };
                drawList->AddRectFilled(valueMin, valueMax, ApplyAlpha(IM_COL32(0, 56, 0, 205), alpha), Scale(2.0f));
                drawList->AddRect(valueMin, valueMax, ApplyAlpha(IM_COL32(0, 130, 0, 180), alpha), Scale(2.0f), 0, Scale(1.0f));
                const ImVec2 valueSize = g_smallFont->CalcTextSizeA(Scale(14.0f), FLT_MAX, 0.0f, valueText.c_str());
                const ImVec2 valuePos{ valueMin.x + std::max(Scale(4.0f), (valueWidth - valueSize.x) * 0.5f),
                    valueMin.y + Scale(3.0f) };
                DrawTextWithMarqueeShadow(g_smallFont, Scale(14.0f), valuePos, valueMin, valueMax,
                    ApplyAlpha(IM_COL32(190, 255, 128, 255), alpha), valueText.c_str(), g_optionSelectionTime, 1.0, 32.0,
                    1.0f, 0.5f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
            }

            if (showCount < mod->options.size())
            {
                const std::string moreText = fmt::format("+{} {}", mod->options.size() - showCount, Localise("Mods_MoreOptions"));
                const float hintY = listTop + static_cast<float>(showCount) * rowHeight + Scale(5.0f);
                DrawTextBasic(g_smallFont, Scale(14.0f), { x + Scale(10.0f), hintY },
                    ApplyAlpha(IM_COL32(175, 215, 160, 255), alpha), moreText.c_str());
            }
            drawList->PopClipRect();
            return;
        }

        const float listTop = min.y + Scale(194.0f);
        const float descriptionY = max.y - Scale(76.0f);
        const float listBottom = descriptionY - Scale(13.0f);
        const float rowHeight = Scale(34.0f);
        const size_t visibleRows = VisibleRows(listBottom - listTop, rowHeight, MAX_VISIBLE_OPTIONS);

        if (g_selectedOptionIndex >= mod->options.size())
            g_selectedOptionIndex = 0;
        if (g_selectedOptionIndex < g_firstVisibleOptionIndex)
            g_firstVisibleOptionIndex = g_selectedOptionIndex;
        else if (g_selectedOptionIndex >= g_firstVisibleOptionIndex + visibleRows)
            g_firstVisibleOptionIndex = g_selectedOptionIndex - visibleRows + 1;
        const size_t maxFirstVisible = mod->options.size() > visibleRows ? mod->options.size() - visibleRows : 0;
        g_firstVisibleOptionIndex = std::min(g_firstVisibleOptionIndex, maxFirstVisible);
        g_optionScrollPosition = SmoothScroll(g_optionScrollPosition,
            static_cast<float>(g_firstVisibleOptionIndex), visibleRows);

        const size_t first = std::min(static_cast<size_t>(std::floor(g_optionScrollPosition)), mod->options.size() - 1);
        const size_t last = std::min(mod->options.size(), first + visibleRows + 1);
        const float listLeft = min.x + Scale(10.0f);
        const float listRight = max.x - Scale(18.0f);
        drawList->PushClipRect({ listLeft, listTop }, { listRight, listBottom }, true);
        for (size_t index = first; index < last; ++index)
        {
            const float rowY = listTop + (static_cast<float>(index) - g_optionScrollPosition) * rowHeight;
            const ImVec2 rowMin{ listLeft, rowY };
            const ImVec2 rowMax{ listRight, rowY + rowHeight - Scale(2.0f) };
            drawList->AddRectFilled(rowMin, rowMax, ApplyAlpha(IM_COL32(0, 22, 0, 135), alpha), Scale(2.0f));
            if (index == g_selectedOptionIndex)
                DrawOptionsSelection(rowMin, rowMax, alpha);
            else
                drawList->AddRectFilled({ rowMin.x, rowMin.y + Scale(7.0f) },
                    { rowMin.x + Scale(3.0f), rowMax.y - Scale(7.0f) }, ApplyAlpha(IM_COL32(0, 125, 0, 180), alpha));

            const auto& option = mod->options[index];
            const std::string label = option.displayName.empty() ? option.name : option.displayName;
            const std::string valueText = GetChoiceDisplayName(option);
            const float valueWidth = std::min(Scale(172.0f), (rowMax.x - rowMin.x) * 0.42f);
            const float valueLeft = rowMax.x - valueWidth - Scale(8.0f);
            DrawTextWithMarqueeShadow(g_textFont, Scale(17.0f),
                { rowMin.x + Scale(16.0f), rowMin.y + Scale(9.0f) },
                { rowMin.x + Scale(8.0f), rowMin.y }, { valueLeft - Scale(8.0f), rowMax.y },
                ApplyAlpha(index == g_selectedOptionIndex ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 220, 205, 255), alpha),
                label.c_str(), g_optionSelectionTime, 1.0, 38.0, 2.0f, 1.0f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));

            const ImVec2 valueMin{ valueLeft, rowMin.y + Scale(4.0f) };
            const ImVec2 valueMax{ rowMax.x - Scale(3.0f), rowMax.y - Scale(4.0f) };
            drawList->AddRectFilled(valueMin, valueMax, ApplyAlpha(IM_COL32(0, 48, 0, 220), alpha), Scale(2.0f));
            drawList->AddRect(valueMin, valueMax, ApplyAlpha(IM_COL32(0, 125, 0, 180), alpha), Scale(2.0f), 0, Scale(1.0f));
            const ImVec2 valueSize = g_smallFont->CalcTextSizeA(Scale(14.0f), FLT_MAX, 0.0f, valueText.c_str());
            const ImVec2 valuePos{ valueMin.x + std::max(Scale(4.0f), (valueWidth - valueSize.x) * 0.5f),
                valueMin.y + Scale(3.0f) };
            DrawTextWithMarqueeShadow(g_smallFont, Scale(14.0f), valuePos, valueMin, valueMax,
                ApplyAlpha(IM_COL32(190, 255, 128, 255), alpha), valueText.c_str(), g_optionSelectionTime, 1.0, 32.0,
                1.0f, 0.5f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
        }
        drawList->PopClipRect();

        DrawScrollBar(drawList,
            { max.x - Scale(11.0f), listTop },
            { max.x - Scale(5.0f), listBottom },
            mod->options.size(), visibleRows, g_optionScrollPosition, alpha);

        if (g_selectedOptionIndex < mod->options.size())
        {
            const auto& option = mod->options[g_selectedOptionIndex];
            const std::string description = GetSelectedOptionDescription(option);
            const ImVec2 descriptionMin{ x, descriptionY };
            const ImVec2 descriptionMax{ right, max.y - Scale(18.0f) };
            drawList->AddRectFilled(descriptionMin, descriptionMax, ApplyAlpha(IM_COL32(0, 24, 0, 175), alpha), Scale(2.0f));
            drawList->AddRect(descriptionMin, descriptionMax, ApplyAlpha(IM_COL32(0, 89, 0, 185), alpha), Scale(2.0f), 0, Scale(1.0f));
            drawList->PushClipRect(descriptionMin, descriptionMax, true);
            if (!description.empty())
                DrawTextWithMarqueeShadow(g_smallFont, Scale(15.0f), { x + Scale(8.0f), descriptionY + Scale(7.0f) },
                    { x + Scale(7.0f), descriptionY }, { right - Scale(7.0f), descriptionMax.y },
                    ApplyAlpha(IM_COL32(190, 215, 195, 255), alpha), description.c_str(), g_optionSelectionTime, 1.2, 36.0,
                    1.0f, 0.5f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
            else if (option.choices.empty())
                DrawTextBasic(g_smallFont, Scale(15.0f), { x + Scale(8.0f), descriptionY + Scale(7.0f) },
                    ApplyAlpha(IM_COL32(200, 215, 200, 255), alpha), Localise("Mods_UnsupportedOption").c_str());
            drawList->PopClipRect();
        }
    }

    void DrawOptionsStyleBands(ImDrawList* drawList, ImVec2 screen, float alpha)
    {
        const float bandHeight = Scale(105.0f);
        const ImU32 fadeTop = ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha);
        const ImU32 fadeClear = ApplyAlpha(IM_COL32(0, 0, 0, 0), alpha);
        drawList->AddRectFilledMultiColor({ 0.0f, 0.0f }, { screen.x, bandHeight },
            fadeTop, fadeTop, fadeClear, fadeClear);
        drawList->AddRectFilledMultiColor({ 0.0f, screen.y - bandHeight }, screen,
            fadeClear, fadeClear, fadeTop, fadeTop);

        SetShaderModifier(IMGUI_SHADER_MODIFIER_SCANLINE);
        const ImU32 bandClear = ApplyAlpha(IM_COL32(203, 255, 0, 0), alpha);
        const ImU32 bandColor = ApplyAlpha(IM_COL32(203, 255, 0, 55), alpha);
        drawList->AddRectFilledMultiColor({ 0.0f, 0.0f }, { screen.x, bandHeight },
            bandClear, bandClear, bandColor, bandColor);
        SetProceduralOrigin({ 0.0f, screen.y - bandHeight });
        drawList->AddRectFilledMultiColor({ 0.0f, screen.y - bandHeight }, screen,
            bandColor, bandColor, bandClear, bandClear);
        ResetProceduralOrigin();
        SetShaderModifier(IMGUI_SHADER_MODIFIER_NONE);

        drawList->AddRectFilled({ 0.0f, bandHeight - Scale(1.0f) }, { screen.x, bandHeight + Scale(1.0f) },
            ApplyAlpha(IM_COL32(115, 178, 104, 210), alpha));
        drawList->AddRectFilled({ 0.0f, screen.y - bandHeight - Scale(1.0f) },
            { screen.x, screen.y - bandHeight + Scale(1.0f) }, ApplyAlpha(IM_COL32(115, 178, 104, 210), alpha));
    }

    void DrawMenu()
    {
        auto drawList = ImGui::GetBackgroundDrawList();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        const float motion = IntroProgress();
        const float alpha = motion;
        drawList->AddRectFilled({ 0.0f, 0.0f }, screen, ApplyAlpha(IM_COL32(0, 0, 0, 190), alpha));
        DrawOptionsStyleBands(drawList, screen, alpha);

        float titleX = g_aspectRatioOffsetX + Scale(122.0f);
        if (g_aspectRatio >= WIDE_ASPECT_RATIO)
            titleX += g_aspectRatioOffsetX;
        else
            titleX += (1.0f - g_aspectRatioNarrowScale) * g_aspectRatioScale * -20.0f;
        const float titleY = g_aspectRatioOffsetY + Scale(56.0f) - Scale(8.0f) * (1.0f - motion);
        DrawTextWithOutline(g_titleFont, Scale(48.0f), { titleX, titleY },
            ApplyAlpha(IM_COL32(255, 190, 33, 255), alpha), Localise("Mods_Header_Title").c_str(),
            Scale(4.0f), ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha), IMGUI_SHADER_MODIFIER_TITLE_BEVEL);
        DrawVersionString(g_smallFont, ApplyAlpha(IM_COL32(255, 255, 255, 70), alpha));

        const float horizontalMargin = g_aspectRatioOffsetX + Scale(70.0f);
        const float top = g_aspectRatioOffsetY + Scale(117.0f) + Scale(14.0f) * (1.0f - motion);
        const float panelBottom = g_aspectRatioOffsetY + Scale(604.0f);
        const float panelHeight = std::max(Scale(360.0f), panelBottom - top);
        const float totalWidth = screen.x - horizontalMargin * 2.0f;
        const float gap = Scale(14.0f);
        const float listWidth = (totalWidth - gap) * 0.40f;
        const ImVec2 listMin{ horizontalMargin, top };
        const ImVec2 listMax{ listMin.x + listWidth, top + panelHeight };
        const ImVec2 detailMin{ listMax.x + gap, top };
        const ImVec2 detailMax{ screen.x - horizontalMargin, top + panelHeight };

        DrawModList(drawList, listMin, listMax, alpha);
        DrawModDetails(drawList, detailMin, detailMax, alpha);

        const std::filesystem::path modsPath = GetUserPath() / "mods";
        const std::string pathText = fmt::format("{}: {}", Localise("Mods_Folder"), modsPath.string());
        const float pathSize = Scale(13.0f);
        DrawTextWithMarqueeShadow(g_smallFont, pathSize,
            { horizontalMargin, screen.y - pathSize - Scale(17.0f) },
            { horizontalMargin, screen.y - pathSize - Scale(19.0f) },
            { screen.x - horizontalMargin, screen.y - Scale(7.0f) },
            ApplyAlpha(IM_COL32(165, 205, 170, 220), alpha), pathText.c_str(), g_appearTime, 1.0, 30.0,
            1.0f, 0.5f, ApplyAlpha(IM_COL32(0, 0, 0, 255), alpha));
    }
}

void ModsMenu::Init()
{
    g_titleFont = ImFontAtlasSnapshot::GetFont("DFSoGeiStd-W7.otf");
    g_textFont = ImFontAtlasSnapshot::GetFont("FOT-SeuratPro-M.otf");
    g_smallFont = ImFontAtlasSnapshot::GetFont("FOT-NewRodinPro-DB.otf");
}

void ModsMenu::Draw()
{
    if (!s_isVisible)
        return;

    ProcessInput();
    if (s_isVisible)
        DrawMenu();
}

void ModsMenu::Open()
{
    g_originalMods = ModLoader::GetUserMods();
    g_workingMods = g_originalMods;
    g_selectedModIndex = 0;
    g_selectedOptionIndex = 0;
    g_firstVisibleModIndex = 0;
    g_firstVisibleOptionIndex = 0;
    g_modScrollPosition = 0.0f;
    g_optionScrollPosition = 0.0f;
    g_appearTime = ImGui::GetTime();
    g_modSelectionTime = g_appearTime;
    g_optionSelectionTime = g_appearTime;
    g_isMovingMod = false;
    g_isEditingOptions = false;
    g_errorMessage.clear();
    s_isRestartRequired = false;
    s_isVisible = true;

    const auto& pad = SWA::CInputState::GetInstance()->GetPadState();
    g_inputBuffered = pad.IsDown(SWA::eKeyState_A) || pad.IsDown(SWA::eKeyState_Start) || pad.IsDown(SWA::eKeyState_X);

    UpdateButtonGuide();
    hid::SetProhibitedInputs(XAMINPUT_GAMEPAD_START, false, true);
}

void ModsMenu::Close()
{
    if (!s_isVisible)
        return;

    s_isVisible = false;
    g_isMovingMod = false;
    g_isEditingOptions = false;
    s_isRestartRequired = HasChanges();
    if (!s_isRestartRequired)
        g_workingMods = g_originalMods;

    ButtonGuide::Close();
    hid::SetProhibitedInputs();
}

bool ModsMenu::CommitRestartSettings()
{
    std::string errorMessage;
    if (!ModLoader::CommitUserMods(g_workingMods, errorMessage))
    {
        g_errorMessage = errorMessage.empty() ? Localise("Mods_Error_SaveFailed") : std::move(errorMessage);
        s_isRestartRequired = false;
        s_isVisible = true;
        g_isMovingMod = false;
        g_isEditingOptions = false;
        g_inputBuffered = true;
        UpdateButtonGuide();
        hid::SetProhibitedInputs(XAMINPUT_GAMEPAD_START, false, true);
        return false;
    }

    g_originalMods = g_workingMods;
    s_isRestartRequired = false;
    g_errorMessage.clear();
    return true;
}

void ModsMenu::RevertRestartSettings()
{
    g_workingMods = g_originalMods;
    s_isRestartRequired = false;
    g_isMovingMod = false;
    g_isEditingOptions = false;
    g_errorMessage.clear();
}
