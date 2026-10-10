#include "app.h"
#include <api/SWA.h>
#include <gpu/video.h>
#include <install/installer.h>
#include <kernel/function.h>
#include <mod/mod_loader.h>
#include <os/logger.h>
#include <os/process.h>
#include <patches/audio_patches.h>
#include <patches/inspire_patches.h>
#include <ui/fader.h>
#include <ui/game_window.h>
#include <ui/options_menu.h>
#include <user/config.h>
#include <user/paths.h>
#include <user/registry.h>
#include <atomic>
#include <cstdio>

static std::atomic<bool> g_pendingLanguageSync = false;
static ELanguage g_lastAppliedLanguage = ELanguage::English;
static EVoiceLanguage g_lastAppliedVoiceLanguage = EVoiceLanguage::English;

uint32_t App::ToSwaLanguage(ELanguage lang)
{
    switch (lang)
    {
        case ELanguage::English:  return SWA::eLanguage_English;
        case ELanguage::Japanese: return SWA::eLanguage_Japanese;
        case ELanguage::French:   return SWA::eLanguage_French;
        case ELanguage::German:   return SWA::eLanguage_German;
        case ELanguage::Italian:  return SWA::eLanguage_Italian;
        case ELanguage::Spanish:  return SWA::eLanguage_Spanish;
        default:                  return SWA::eLanguage_English;
    }
}

void App::NotifyLanguageChanged()
{
    s_language = Config::Language;
    g_pendingLanguageSync.store(true, std::memory_order_release);
}

void App::Restart(std::vector<std::string> restartArgs)
{
#if defined(__PROSPERO__)
    (void)restartArgs;
    // Keep the legacy in-process reboot for non-settings callers. Restart-required
    // settings use RestartForSettings() and leave through the PS5 shell instead.
    // Refresh the virtual mod overlay from the just-committed ModsDB.ini and
    // mod.ini values before this process continues.
    ModLoader::Init();
    s_language = Config::Language;
    NotifyLanguageChanged();
    Config::Save();
    Registry::Save();
    OptionsMenu::s_isRestartRequired = false;
    s_isSoftRebootRequested = true;
#else
    os::process::StartProcess(os::process::GetExecutablePath(), restartArgs, os::process::GetWorkingDirectory());
    Exit();
#endif
}

void App::RestartForSettings()
{
#if defined(__PROSPERO__)
    // The settings menu commits Config/mod data before this call. Preserve the
    // executable-path registry entry, then close through App::Exit's shell path.
    Registry::Save();
    OptionsMenu::s_isRestartRequired = false;
    Exit();
#else
    Restart();
#endif
}

#if defined(__PROSPERO__)
extern "C" int sceSystemServiceLoadExec(const char* path, const char *const argv[]);
#endif

void App::Exit()
{
    Config::Save();

#ifdef _WIN32
    timeEndPeriod(1);
#endif

#if defined(__PROSPERO__)
    // Native title shutdown must go through the shell; kernel exit(0) raises SIGSYS.
    std::fflush(nullptr);
    const int result = sceSystemServiceLoadExec("exit", nullptr);
    if (result >= 0)
    {
        // The shell terminates the title asynchronously after accepting the request.
        for (;;)
            usleep(100000);
    }
    else
    {
        LOGF_ERROR("sceSystemServiceLoadExec(exit) failed: {}", result);
    }
#else
    std::_Exit(0);
#endif
}

// SWA::CApplication::CApplication
PPC_FUNC_IMPL(__imp__sub_824EB490);
PPC_FUNC(sub_824EB490)
{
    App::s_isInit = true;
    App::s_isMissingDLC = !Installer::checkAllDLC(GetGamePath());
    App::s_language = Config::Language;
    g_lastAppliedLanguage = Config::Language.Value;
    g_lastAppliedVoiceLanguage = Config::VoiceLanguage.Value;

    SWA::SGlobals::Init();
    Registry::Save();

    __imp__sub_824EB490(ctx, base);
}

static std::thread::id g_mainThreadId = std::this_thread::get_id();

// SWA::CApplication::Update
PPC_FUNC_IMPL(__imp__sub_822C1130);
PPC_FUNC(sub_822C1130)
{
    Video::WaitOnSwapChain();

    // Correct small delta time errors.
    if (Config::FPS >= FPS_MIN && Config::FPS < FPS_MAX)
    {
        double targetDeltaTime = 1.0 / Config::FPS;

        if (abs(ctx.f1.f64 - targetDeltaTime) < 0.00001)
            ctx.f1.f64 = targetDeltaTime;
    }

    App::s_deltaTime = ctx.f1.f64;
    App::s_time += App::s_deltaTime;

    // This function can also be called by the loading thread,
    // which SDL does not like. To prevent the OS from thinking
    // the process is unresponsive, we will flush while waiting
    // for the pipelines to finish compiling in video.cpp.
    const bool isMainThread = (std::this_thread::get_id() == g_mainThreadId);
    if (isMainThread)
    {
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
        GameWindow::Update();
    }

    AudioPatches::Update(App::s_deltaTime);
    InspirePatches::Update();

    // Apply subtitles and committed language/voice options.
    if (auto pApplicationDocument = SWA::CApplicationDocument::GetInstance())
    {
        pApplicationDocument->m_InspireSubtitles = Config::Subtitles;

        if (!OptionsMenu::s_isVisible && !OptionsMenu::s_isRestartRequired)
        {
            const auto targetLang = (SWA::ELanguage)App::ToSwaLanguage(Config::Language.Value);
            const auto targetVoiceLang = (SWA::EVoiceLanguage)Config::VoiceLanguage.Value;
            const auto targetRegion = (Config::Language == ELanguage::Japanese)
                ? SWA::eRegion_Japan
                : SWA::eRegion_RestOfWorld;

            pApplicationDocument->m_Language = targetLang;
            pApplicationDocument->m_VoiceLanguage = targetVoiceLang;
            pApplicationDocument->m_Region = targetRegion;

            if (isMainThread && !App::s_isLoading)
            {
                const bool langChanged = (g_lastAppliedLanguage != Config::Language.Value);
                const bool voiceChanged = (g_lastAppliedVoiceLanguage != Config::VoiceLanguage.Value);
                const bool pendingSync = g_pendingLanguageSync.exchange(false, std::memory_order_acq_rel);

                if (pendingSync || langChanged || voiceChanged)
                {
                    App::s_language = Config::Language;
                    g_lastAppliedLanguage = Config::Language.Value;
                    g_lastAppliedVoiceLanguage = Config::VoiceLanguage.Value;

                    GuestToHostFunction<void>(sub_825198C8, &pApplicationDocument->m_Language);
                }
            }
        }
    }

    if (Config::EnableEventCollisionDebugView)
        *SWA::SGlobals::ms_IsTriggerRender = true;

    if (Config::EnableGIMipLevelDebugView)
        *SWA::SGlobals::ms_VisualizeLoadedLevel = true;

    if (Config::EnableObjectCollisionDebugView)
        *SWA::SGlobals::ms_IsObjectCollisionRender = true;

    if (Config::EnableStageCollisionDebugView)
        *SWA::SGlobals::ms_IsCollisionRender = true;

    __imp__sub_822C1130(ctx, base);

    if (auto pApplicationDocument = SWA::CApplicationDocument::GetInstance())
    {
        pApplicationDocument->m_InspireSubtitles = Config::Subtitles;

        if (!OptionsMenu::s_isVisible && !OptionsMenu::s_isRestartRequired)
        {
            pApplicationDocument->m_Language = (SWA::ELanguage)App::ToSwaLanguage(Config::Language.Value);
            pApplicationDocument->m_VoiceLanguage = (SWA::EVoiceLanguage)Config::VoiceLanguage.Value;
            pApplicationDocument->m_Region = (Config::Language == ELanguage::Japanese)
                ? SWA::eRegion_Japan
                : SWA::eRegion_RestOfWorld;
        }
    }
}

