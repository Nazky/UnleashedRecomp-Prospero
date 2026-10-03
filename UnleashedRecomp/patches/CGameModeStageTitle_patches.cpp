#include <api/SWA.h>
#include <kernel/function.h>
#include <patches/CTitleStateIntro_patches.h>
#include <ui/fader.h>
#include <user/config.h>
#include <app.h>

// SWA::CGameModeStageTitle::Update
PPC_FUNC_IMPL(__imp__sub_825518B8);
PPC_FUNC(sub_825518B8)
{
    if (App::s_isSoftRebootRequested && !App::s_isLoading)
    {
        App::s_isSoftRebootRequested = false;

        auto r3 = ctx.r3;

        if (auto pApplicationDocument = SWA::CApplicationDocument::GetInstance())
        {
            pApplicationDocument->m_Language = (SWA::ELanguage)App::ToSwaLanguage(Config::Language.Value);
            pApplicationDocument->m_VoiceLanguage = (SWA::EVoiceLanguage)Config::VoiceLanguage.Value;
            pApplicationDocument->m_Region = (Config::Language == ELanguage::Japanese)
                ? SWA::eRegion_Japan
                : SWA::eRegion_RestOfWorld;
            pApplicationDocument->m_InspireSubtitles = Config::Subtitles;

            GuestToHostFunction<void>(sub_825198C8, &pApplicationDocument->m_Language);
        }

        ctx.r3 = r3;
        ctx.r4.u64 = 0;
        ctx.r5.u64 = 0;
        ctx.r6.u64 = 1;
        ctx.r7.u64 = 0;
        sub_825517C8(ctx, base);

        Fader::FadeIn(1);
        return;
    }

    auto pGameModeStageTitle = (SWA::CGameModeStageTitle*)g_memory.Translate(ctx.r3.u32);

    __imp__sub_825518B8(ctx, base);

    if (g_quitMessageOpen)
        pGameModeStageTitle->m_AdvertiseMovieWaitTime = 0;
}
