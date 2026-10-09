#include "mods_menu.h"

#include <api/SWA.h>
#include <exports.h>
#include <gpu/imgui/imgui_snapshot.h>
#include <hid/hid.h>
#include <locale/locale.h>
#include <ui/button_guide.h>
#include <ui/imgui_utils.h>
#include <user/paths.h>

#include <algorithm>
#include <array>
#include <cfloat>
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
                g_errorMessage.clear();
                UpdateButtonGuide();
                Game_PlaySound("sys_worldmap_decide");
            }
        }
    }

    void DrawPanelHeader(ImVec2 min, ImVec2 max, std::string_view text)
    {
        DrawPauseHeaderContainer(min, max);
        DrawTextWithOutline(
            g_titleFont,
            Scale(21.0f),
            { min.x + Scale(22.0f), min.y + Scale(8.0f) },
            IM_COL32(255, 202, 66, 255),
            std::string(text).c_str(),
            Scale(1.5f),
            IM_COL32(0, 0, 0, 255));
    }

    void DrawModList(ImDrawList* drawList, ImVec2 min, ImVec2 max)
    {
        DrawPanelHeader(min, { max.x, min.y + Scale(42.0f) }, Localise("Mods_Panel_List"));
        DrawPauseContainer({ min.x, min.y + Scale(42.0f) }, max);

        const float inset = Scale(18.0f);
        const float rowHeight = Scale(42.0f);
        const float rowsTop = min.y + Scale(55.0f);
        const float textSize = Scale(20.0f);
        const size_t modCount = g_workingMods.size();
        if (modCount == 0)
        {
            DrawTextBasic(g_textFont, textSize, { min.x + inset, rowsTop + Scale(8.0f) },
                IM_COL32(230, 230, 230, 255), Localise("Mods_Empty").c_str());
            return;
        }

        if (g_selectedModIndex < g_firstVisibleModIndex)
            g_firstVisibleModIndex = g_selectedModIndex;
        else if (g_selectedModIndex >= g_firstVisibleModIndex + MAX_VISIBLE_MODS)
            g_firstVisibleModIndex = g_selectedModIndex - MAX_VISIBLE_MODS + 1;

        const size_t last = std::min(modCount, g_firstVisibleModIndex + MAX_VISIBLE_MODS);
        drawList->PushClipRect({ min.x + Scale(8.0f), rowsTop }, { max.x - Scale(8.0f), max.y - Scale(15.0f) }, true);
        for (size_t index = g_firstVisibleModIndex; index < last; ++index)
        {
            const float rowY = rowsTop + static_cast<float>(index - g_firstVisibleModIndex) * rowHeight;
            const ImVec2 rowMin{ min.x + Scale(8.0f), rowY };
            const ImVec2 rowMax{ max.x - Scale(8.0f), rowY + rowHeight };
            if (index == g_selectedModIndex)
                DrawSelectionContainer(rowMin, rowMax);

            const std::string& folderName = g_workingMods[index].folderName;
            DrawTextWithMarqueeShadow(
                g_textFont,
                textSize,
                { rowMin.x + inset, rowMin.y + Scale(9.0f) },
                { rowMin.x + inset, rowMin.y },
                { rowMax.x - Scale(20.0f), rowMax.y },
                index == g_selectedModIndex ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 210, 210, 255),
                folderName.c_str(),
                0.0,
                1.2,
                45.0);

            if (index == g_selectedModIndex && g_isMovingMod)
            {
                const std::string movingText = Localise("Mods_Moving");
                const ImVec2 movingSize = g_smallFont->CalcTextSizeA(Scale(15.0f), FLT_MAX, 0.0f, movingText.c_str());
                DrawTextBasic(g_smallFont, Scale(15.0f), { rowMax.x - movingSize.x - Scale(15.0f), rowMin.y + Scale(12.0f) },
                    IM_COL32(255, 201, 52, 255), movingText.c_str());
            }
        }
        drawList->PopClipRect();

        const std::string countText = fmt::format("{} / {}", modCount == 0 ? 0 : g_selectedModIndex + 1, modCount);
        const ImVec2 countSize = g_smallFont->CalcTextSizeA(Scale(14.0f), FLT_MAX, 0.0f, countText.c_str());
        DrawTextBasic(g_smallFont, Scale(14.0f), { max.x - countSize.x - Scale(18.0f), max.y - Scale(28.0f) },
            IM_COL32(170, 185, 195, 255), countText.c_str());
    }

    void DrawModDetails(ImDrawList* drawList, ImVec2 min, ImVec2 max)
    {
        DrawPanelHeader(min, { max.x, min.y + Scale(42.0f) },
            g_isEditingOptions ? Localise("Mods_Panel_Options") : Localise("Mods_Panel_Details"));
        DrawPauseContainer({ min.x, min.y + Scale(42.0f) }, max);

        const UserModInfo* mod = GetSelectedMod();
        if (mod == nullptr)
        {
            DrawTextBasic(g_textFont, Scale(20.0f), { min.x + Scale(22.0f), min.y + Scale(72.0f) },
                IM_COL32(220, 220, 220, 255), Localise("Mods_SelectPrompt").c_str());
            return;
        }

        const float x = min.x + Scale(24.0f);
        const float right = max.x - Scale(24.0f);
        float y = min.y + Scale(57.0f);
        const float bodyTextSize = Scale(17.0f);
        const float titleTextSize = Scale(23.0f);
        const ImVec2 bodyClipMin{ x, y };
        const ImVec2 bodyClipMax{ right, min.y + Scale(152.0f) };
        drawList->PushClipRect(bodyClipMin, bodyClipMax, true);

        const std::string& title = mod->title.empty() ? mod->folderName : mod->title;
        DrawTextWithMarqueeShadow(g_titleFont, titleTextSize, { x, y }, bodyClipMin, bodyClipMax,
            IM_COL32(255, 208, 74, 255), title.c_str(), 0.0, 1.0, 42.0);
        y += Scale(32.0f);

        if (!mod->author.empty())
        {
            const std::string authorText = fmt::format("{} {}", Localise("Mods_Author"), mod->author);
            DrawTextWithMarqueeShadow(g_smallFont, Scale(15.0f), { x, y }, bodyClipMin, bodyClipMax,
                IM_COL32(180, 195, 205, 255), authorText.c_str(), 0.0, 1.0, 38.0);
            y += Scale(22.0f);
        }

        if (!mod->description.empty())
        {
            drawList->PushClipRect({ x, y }, bodyClipMax, true);
            DrawTextWithMarqueeShadow(g_textFont, bodyTextSize, { x, y }, { x, y }, bodyClipMax,
                IM_COL32(220, 225, 230, 255), mod->description.c_str(), 0.0, 1.5, 34.0);
            drawList->PopClipRect();
        }
        drawList->PopClipRect();

        if (!g_errorMessage.empty())
        {
            const float errorY = min.y + Scale(148.0f);
            drawList->PushClipRect({ x, errorY }, { right, errorY + Scale(38.0f) }, true);
            DrawTextBasic(g_smallFont, Scale(15.0f), { x, errorY }, IM_COL32(255, 120, 105, 255), g_errorMessage.c_str());
            drawList->PopClipRect();
        }

        if (!g_isEditingOptions)
        {
            if (mod->options.empty())
            {
                DrawTextBasic(g_textFont, bodyTextSize, { x, min.y + Scale(192.0f) },
                    IM_COL32(180, 190, 200, 255), Localise("Mods_NoOptions").c_str());
            }
            else
            {
                DrawTextBasic(g_textFont, bodyTextSize, { x, min.y + Scale(192.0f) },
                    IM_COL32(255, 210, 80, 255), fmt::format("{}: {}", Localise("Mods_OptionCount"), mod->options.size()).c_str());
                float optionY = min.y + Scale(224.0f);
                const size_t showCount = std::min<size_t>(3, mod->options.size());
                for (size_t i = 0; i < showCount; ++i)
                {
                    const auto& option = mod->options[i];
                    const std::string valueText = GetChoiceDisplayName(option);
                    const std::string rowText = fmt::format("{}: {}", option.displayName, valueText);
                    DrawTextWithMarqueeShadow(g_textFont, bodyTextSize, { x, optionY }, { x, optionY }, { right, optionY + Scale(30.0f) },
                        IM_COL32(220, 225, 230, 255), rowText.c_str(), 0.0, 1.0, 40.0);
                    optionY += Scale(34.0f);
                }
            }
            return;
        }

        if (mod->options.empty())
        {
            DrawTextBasic(g_textFont, bodyTextSize, { x, min.y + Scale(192.0f) },
                IM_COL32(180, 190, 200, 255), Localise("Mods_NoOptions").c_str());
            return;
        }

        const float listTop = min.y + Scale(194.0f);
        const float rowHeight = Scale(38.0f);
        if (g_selectedOptionIndex < g_firstVisibleOptionIndex)
            g_firstVisibleOptionIndex = g_selectedOptionIndex;
        else if (g_selectedOptionIndex >= g_firstVisibleOptionIndex + MAX_VISIBLE_OPTIONS)
            g_firstVisibleOptionIndex = g_selectedOptionIndex - MAX_VISIBLE_OPTIONS + 1;

        const size_t last = std::min(mod->options.size(), g_firstVisibleOptionIndex + MAX_VISIBLE_OPTIONS);
        drawList->PushClipRect({ min.x + Scale(8.0f), listTop }, { max.x - Scale(8.0f), max.y - Scale(12.0f) }, true);
        for (size_t index = g_firstVisibleOptionIndex; index < last; ++index)
        {
            const float rowY = listTop + static_cast<float>(index - g_firstVisibleOptionIndex) * rowHeight;
            const ImVec2 rowMin{ min.x + Scale(8.0f), rowY };
            const ImVec2 rowMax{ max.x - Scale(8.0f), rowY + rowHeight };
            if (index == g_selectedOptionIndex)
                DrawSelectionContainer(rowMin, rowMax);

            const auto& option = mod->options[index];
            const std::string valueText = GetChoiceDisplayName(option);
            const std::string rowText = fmt::format("{}: {}", option.displayName, valueText);
            DrawTextWithMarqueeShadow(g_textFont, Scale(17.0f), { rowMin.x + Scale(18.0f), rowMin.y + Scale(8.0f) },
                { rowMin.x + Scale(18.0f), rowMin.y }, { rowMax.x - Scale(16.0f), rowMax.y },
                index == g_selectedOptionIndex ? IM_COL32(255, 255, 255, 255) : IM_COL32(205, 215, 225, 255),
                rowText.c_str(), 0.0, 1.0, 44.0);
        }
        drawList->PopClipRect();

        if (g_selectedOptionIndex < mod->options.size())
        {
            const auto& option = mod->options[g_selectedOptionIndex];
            const std::string description = GetSelectedOptionDescription(option);
            const float descriptionY = max.y - Scale(78.0f);
            drawList->PushClipRect({ x, descriptionY }, { right, max.y - Scale(24.0f) }, true);
            if (!description.empty())
                DrawTextWithMarqueeShadow(g_smallFont, Scale(15.0f), { x, descriptionY }, { x, descriptionY },
                    { right, max.y - Scale(24.0f) }, IM_COL32(180, 195, 205, 255), description.c_str(), 0.0, 1.2, 36.0);
            else if (option.choices.empty())
                DrawTextBasic(g_smallFont, Scale(15.0f), { x, descriptionY }, IM_COL32(200, 200, 200, 255),
                    Localise("Mods_UnsupportedOption").c_str());
            drawList->PopClipRect();
        }
    }

    void DrawMenu()
    {
        auto drawList = ImGui::GetBackgroundDrawList();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        drawList->AddRectFilled({ 0.0f, 0.0f }, screen, IM_COL32(0, 0, 0, 190));

        DrawTextWithOutline(
            g_titleFont,
            Scale(42.0f),
            { Scale(82.0f), Scale(42.0f) },
            IM_COL32(255, 193, 35, 255),
            Localise("Mods_Header_Title").c_str(),
            Scale(3.0f),
            IM_COL32(0, 0, 0, 255));

        const float horizontalMargin = Scale(70.0f);
        const float top = Scale(112.0f);
        const float bottomMargin = Scale(82.0f);
        const float panelHeight = std::max(Scale(360.0f), screen.y - top - bottomMargin);
        const float totalWidth = screen.x - horizontalMargin * 2.0f;
        const float gap = Scale(14.0f);
        const float listWidth = (totalWidth - gap) * 0.40f;
        const ImVec2 listMin{ horizontalMargin, top };
        const ImVec2 listMax{ listMin.x + listWidth, top + panelHeight };
        const ImVec2 detailMin{ listMax.x + gap, top };
        const ImVec2 detailMax{ screen.x - horizontalMargin, top + panelHeight };

        DrawModList(drawList, listMin, listMax);
        DrawModDetails(drawList, detailMin, detailMax);

        const std::filesystem::path modsPath = GetUserPath() / "mods";
        const std::string pathText = fmt::format("{}: {}", Localise("Mods_Folder"), modsPath.string());
        const ImVec2 pathSize = g_smallFont->CalcTextSizeA(Scale(13.0f), FLT_MAX, 0.0f, pathText.c_str());
        DrawTextBasic(g_smallFont, Scale(13.0f), { horizontalMargin, screen.y - pathSize.y - Scale(17.0f) },
            IM_COL32(165, 180, 190, 220), pathText.c_str());
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
