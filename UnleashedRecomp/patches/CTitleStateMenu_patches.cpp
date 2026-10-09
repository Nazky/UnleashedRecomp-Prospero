#include <api/SWA.h>
#include <cpu/guest_stack_var.h>
#include <locale/locale.h>
#include <os/logger.h>
#include <ui/button_guide.h>
#include <ui/fader.h>
#include <ui/message_window.h>
#include <ui/mods_menu.h>
#include <ui/options_menu.h>
#include <user/achievement_manager.h>
#include <user/paths.h>
#include <app.h>
#include <exports.h>
#include <hid/hid.h>

static bool g_installMessageOpen = false;
static bool g_installMessageFaderBegun = false;
static int g_installMessageResult = -1;
static bool g_restartFaderBegun = false;
static int g_restartMessageResult = -1;

static bool ProcessInstallMessage()
{
    if (!g_installMessageOpen)
        return false;

    if (g_installMessageFaderBegun)
        return true;

    auto& str = App::s_isMissingDLC
        ? Localise("Installer_Message_TitleMissingDLC")
        : Localise("Installer_Message_Title");

    std::array<std::string, 2> options = { Localise("Common_Yes"), Localise("Common_No") };

    if (MessageWindow::Open(str, &g_installMessageResult, options, 1) == MSG_CLOSED)
    {
        switch (g_installMessageResult)
        {
            case 0:
                Fader::FadeOut(1, []() { App::Restart({ "--install-dlc" }); });
                g_installMessageFaderBegun = true;
                break;

            case 1:
                g_installMessageOpen = false;
                g_installMessageResult = -1;
                break;
        }
    }

    return true;
}

// SWA::CTitleStateMenu::Update
PPC_FUNC_IMPL(__imp__sub_825882B8);
PPC_FUNC(sub_825882B8)
{
    auto pTitleStateMenu = (SWA::CTitleStateMenu*)g_memory.Translate(ctx.r3.u32);
    auto pGameDocument = SWA::CGameDocument::GetInstance();

    auto pInputState = SWA::CInputState::GetInstance();
    auto& pPadState = pInputState->GetPadState();
    auto isAccepted = pPadState.IsTapped(SWA::eKeyState_A) || pPadState.IsTapped(SWA::eKeyState_Start);

    auto pContext = pTitleStateMenu->GetContextBase<SWA::CTitleStateMenu::CTitleStateMenuContext>();
    const auto cursorIndexBeforeUpdate = pContext->m_pTitleMenu->m_CursorIndex;
    if (ModsMenu::s_isVisible)
    {
        ButtonGuide::s_isModsMenuPromptVisible = false;
        return;
    }

    if (pContext && !OptionsMenu::s_isVisible && !OptionsMenu::s_isRestartRequired &&
        !ModsMenu::s_isRestartRequired && !g_restartFaderBegun)
    {
        auto* pCtxBytes = reinterpret_cast<uint8_t*>(pContext);
        const auto swaLang = (SWA::ELanguage)App::ToSwaLanguage(Config::Language.Value);
        const auto swaVoiceLang = (SWA::EVoiceLanguage)Config::VoiceLanguage.Value;
        const auto swaRegion = (Config::Language == ELanguage::Japanese)
            ? SWA::eRegion_Japan
            : SWA::eRegion_RestOfWorld;

        *reinterpret_cast<bool*>(pCtxBytes + 0x154) = (Config::Language == ELanguage::Japanese);
        *reinterpret_cast<be<SWA::ELanguage>*>(pCtxBytes + 0x1F8) = swaLang;
        *reinterpret_cast<be<SWA::EVoiceLanguage>*>(pCtxBytes + 0x1FC) = swaVoiceLang;
        *reinterpret_cast<be<SWA::ERegion>*>(pCtxBytes + 0x208) = swaRegion;
        *reinterpret_cast<bool*>(pCtxBytes + 0x20D) = Config::Subtitles;

        if (auto* pTitleMenu = pContext->m_pTitleMenu.get())
        {
            auto* pMenuBytes = reinterpret_cast<uint8_t*>(pTitleMenu);
            *reinterpret_cast<be<SWA::EVoiceLanguage>*>(pMenuBytes + 0x88) = swaVoiceLang;
            *reinterpret_cast<be<uint32_t>*>(pMenuBytes + 0x8C) = Config::Subtitles ? 0 : 1;
        }
    }

    auto isNewGameIndex = pContext->m_pTitleMenu->m_CursorIndex == 0;
    auto isOptionsIndex = pContext->m_pTitleMenu->m_CursorIndex == 2;
#if defined(__PROSPERO__)
    // Keep the hidden game-owned Install row hidden; Mods is a separate shortcut below the menu.
    auto isInstallIndex = false;
#else
    auto isInstallIndex = pContext->m_pTitleMenu->m_CursorIndex == 3;
#endif
    const bool hasUserMods = !ModLoader::GetUserMods().empty();

    // Always default to New Game with corrupted save data.
    if (App::s_isSaveDataCorrupt && pContext->m_pTitleMenu->m_CursorIndex == 1)
        pContext->m_pTitleMenu->m_CursorIndex = 0;

    if (ModsMenu::s_isRestartRequired)
    {
        std::array<std::string, 2> options = { Localise("Common_Yes"), Localise("Common_No") };

        if (!g_restartFaderBegun && MessageWindow::Open(Localise("Options_Message_RestartConfirm"), &g_restartMessageResult, options, 0, 1) == MSG_CLOSED)
        {
            const int choice = g_restartMessageResult;
            g_restartMessageResult = -1;

            if (choice == 0)
            {
                if (ModsMenu::CommitRestartSettings())
                {
                    g_restartFaderBegun = true;
                    Fader::FadeOut(1, []()
                    {
                        g_restartFaderBegun = false;
                        App::Restart();
                    });
                }
            }
            else
            {
                ModsMenu::RevertRestartSettings();
            }
        }
    }
    else if (hasUserMods && !OptionsMenu::s_isVisible && !OptionsMenu::s_isRestartRequired &&
        !g_restartFaderBegun && !pContext->m_pTitleMenu->m_IsDeleteCheckMessageOpen &&
        !pContext->m_pTitleMenu->m_IsDLCInfoMessageOpen && pPadState.IsTapped(SWA::eKeyState_X))
    {
        Game_PlaySound("sys_worldmap_window");
        Game_PlaySound("sys_worldmap_decide");
        ModsMenu::Open();
    }
    else if (isNewGameIndex && isAccepted)
    {
        if (pContext->m_pTitleMenu->m_IsDeleteCheckMessageOpen &&
            pGameDocument->m_pMember->m_pGeneralWindow->m_SelectedIndex == 1)
        {
            LOGN("Resetting achievements...");

            AchievementManager::Reset();
        }
    }
    else if (!OptionsMenu::s_isVisible && isOptionsIndex)
    {
        if (OptionsMenu::s_isRestartRequired)
        {
            std::array<std::string, 2> options = { Localise("Common_Yes"), Localise("Common_No") };

            if (!g_restartFaderBegun && MessageWindow::Open(Localise("Options_Message_RestartConfirm"), &g_restartMessageResult, options, 0, 1) == MSG_CLOSED)
            {
                const int choice = g_restartMessageResult;
                g_restartMessageResult = -1;

                if (choice == 0)
                {
                    OptionsMenu::CommitRestartSettings();
                    g_restartFaderBegun = true;
                    Fader::FadeOut(1, []()
                    {
                        g_restartFaderBegun = false;
                        App::Restart();
                    });
                }
                else
                {
                    OptionsMenu::RevertRestartSettings();
                }
            }
        }
        else if (!g_restartFaderBegun && isAccepted)
        {
            Game_PlaySound("sys_worldmap_window");
            Game_PlaySound("sys_worldmap_decide");
            OptionsMenu::Open();
        }
    }
    else if (isInstallIndex && isAccepted)
    {
        g_installMessageOpen = true;
    }

#if defined(__PROSPERO__)
    ButtonGuide::s_isModsMenuPromptVisible = hasUserMods &&
        !ModsMenu::s_isVisible && !ModsMenu::s_isRestartRequired &&
        !OptionsMenu::s_isVisible && !OptionsMenu::s_isRestartRequired &&
        !g_restartFaderBegun && !g_installMessageOpen && !isAccepted &&
        !pContext->m_pTitleMenu->m_IsDeleteCheckMessageOpen &&
        !pContext->m_pTitleMenu->m_IsDLCInfoMessageOpen;
#else
    ButtonGuide::s_isModsMenuPromptVisible = false;
#endif

    if (!OptionsMenu::s_isVisible && !OptionsMenu::s_isRestartRequired &&
        !ModsMenu::s_isVisible && !ModsMenu::s_isRestartRequired &&
        !g_restartFaderBegun && !ProcessInstallMessage())
        __imp__sub_825882B8(ctx, base);

    if (pContext->m_pTitleMenu->m_CursorIndex != cursorIndexBeforeUpdate)
        hid::PulseMenuVibration();

    if (isOptionsIndex)
    {
        if (OptionsMenu::CanClose() && pPadState.IsTapped(SWA::eKeyState_B))
        {
            Game_PlaySound("sys_worldmap_cansel");
            OptionsMenu::Close();
        }
    }
}

void TitleMenuRemoveContinueOnCorruptSaveMidAsmHook(PPCRegister& r3)
{
    if (!App::s_isSaveDataCorrupt)
        return;

    r3.u64 = 0;
}

void TitleMenuRemoveStorageDeviceOptionMidAsmHook(PPCRegister& r11)
{
    r11.u32 = 0;
}

void TitleMenuAddInstallOptionMidAsmHook(PPCRegister& r3)
{
#if defined(__PROSPERO__)
    // The Mods shortcut is shown separately below the title menu; don't expose
    // the game's otherwise-hidden Install row on PS5.
    r3.u32 = 0;
#else
    r3.u32 = 1;
#endif
}
