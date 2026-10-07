#include <stdafx.h>
#include <SDL.h>
#include <user/config.h>
#include <hid/hid.h>
#include <os/logger.h>
#include <ui/game_window.h>
#include <kernel/xdm.h>
#include <app.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#if defined(__PROSPERO__)
#include <atomic>
#include <mutex>
#include <pthread.h>
#include <unistd.h>
#endif

static constexpr XAMINPUT_VIBRATION MENU_VIBRATION_PULSE{ 0x8000, 0x6000 };
static constexpr Uint32 MENU_VIBRATION_DURATION_MS = 120;

static uint16_t ScaleVibrationStrength(uint16_t strength, float multiplier)
{
    const float clampedMultiplier = std::clamp(multiplier, 0.0f, 1.0f);
    const uint32_t scaled = static_cast<uint32_t>(static_cast<float>(strength) * clampedMultiplier + 0.5f);
    return static_cast<uint16_t>(std::min(scaled, 65535u));
}

static XAMINPUT_VIBRATION ScaleVibrationByMultiplier(const XAMINPUT_VIBRATION& vibration, float configuredMultiplier)
{
    const float multiplier = std::clamp(
        std::isfinite(configuredMultiplier) ? configuredMultiplier : 1.0f, 0.0f, 1.0f);
    return {
        ScaleVibrationStrength(vibration.wLeftMotorSpeed, multiplier),
        ScaleVibrationStrength(vibration.wRightMotorSpeed, multiplier)
    };
}

#if defined(__PROSPERO__)
struct Ps5ScePadVibrationParam
{
    uint8_t largeMotor;
    uint8_t smallMotor;
};

extern "C"
{
    int sceUserServiceInitialize(const void* params);
    int sceUserServiceGetInitialUser(int32_t* userId);
    int scePadInit(void);
    int scePadOpen(int32_t userId, int32_t type, int32_t index, const void* param);
    int scePadGetHandle(int32_t userId, int32_t type, int32_t index);
    int scePadReadState(int32_t handle, void* data);
    int scePadSetVibrationMode(int32_t handle, int32_t mode);
    int scePadSetVibration(int32_t handle, const Ps5ScePadVibrationParam* param);
    int sceAudioOutInit(void);
    int sceAudioOutOpen(int32_t userId, int32_t type, int32_t index, uint32_t len, uint32_t freq, uint32_t param);
    int sceAudioOutOutput(int32_t handle, const void* ptr);
    int sceAudioOutClose(int32_t handle);
}

struct Ps5ScePadData
{
    uint32_t buttons;
    uint8_t leftStickX;
    uint8_t leftStickY;
    uint8_t rightStickX;
    uint8_t rightStickY;
    uint8_t analogL2;
    uint8_t analogR2;
    uint16_t padding;
    float orientation[4];
    float acceleration[3];
    float angularVelocity[3];
    uint8_t touchData[32];
    uint8_t connected;
    uint64_t timestamp;
    uint8_t ext[64];
};

static constexpr int32_t PS5_VIBRATION_MODE_ADVANCED = 1;
static constexpr int32_t PS5_VIBRATION_MODE_COMPATIBLE = 2;
static constexpr int32_t PS5_AUDIO_PORT_TYPE_VIBRATION = 10;
static constexpr uint32_t PS5_HAPTIC_SAMPLE_RATE = 48000;
static constexpr uint32_t PS5_HAPTIC_FRAMES_PER_OUTPUT = 256;
static constexpr float PS5_BOOST_RUMBLE_MULTIPLIER = 2.0f;
static constexpr uint32_t PS5_HAPTIC_FORMAT_S16_STEREO = 1;
static constexpr uint32_t PS5_HAPTIC_FORMAT_F32_STEREO = 4;
static constexpr uint32_t PS5_AUDIO_OUT_ALREADY_INITIALIZED = 0x8026000eu;
static constexpr int PS5_HAPTIC_OUTPUT_FAILURE_LIMIT = 40;

static int32_t s_ps5PadHandle = -1;
static int32_t s_ps5PadUserId = -1;
static std::atomic<bool> s_ps5CompatibleVibrationMode{ false };
static std::atomic<bool> s_ps5HapticAudioActive{ false };
static std::atomic<bool> s_ps5HapticThreadRunning{ false };
static std::atomic<uint16_t> s_ps5HapticLargeLevel{ 0 };
static std::atomic<uint16_t> s_ps5HapticSmallLevel{ 0 };
static std::atomic<uint16_t> s_ps5HapticRouteLargeLevel{ 0 };
static std::atomic<uint16_t> s_ps5HapticRouteSmallLevel{ 0 };
static std::atomic<uint16_t> s_ps5RumbleFallbackLargeLevel{ 0 };
static std::atomic<uint16_t> s_ps5RumbleFallbackSmallLevel{ 0 };
static std::atomic<bool> s_ps5HapticRequestLogged{ false };
static std::atomic<bool> s_ps5HapticOutputFailureLogged{ false };
static std::mutex s_ps5HapticInitMutex;
// Serializes compatible-mode transitions and scePadSetVibration calls against
// asynchronous haptic-stream fallback in the worker thread.
static std::mutex s_ps5PadVibrationMutex;
static pthread_t s_ps5HapticThread{};
static int32_t s_ps5HapticPort = -1;
static bool s_ps5HapticFloat32Format;
static bool s_ps5HapticAudioInitErrorLogged;
static bool s_ps5HapticPortOpenFailureLogged;
static bool s_ps5HapticModeErrorLogged;
static bool s_ps5HapticThreadStartFailed;
static std::chrono::steady_clock::time_point s_ps5LastHapticPortAttempt;
static bool s_ps5HapticAttemptTimeSet;
static std::atomic<bool> s_ps5VibrationModeErrorLogged{ false };
static std::atomic<bool> s_ps5VibrationErrorLogged{ false };
static std::atomic<bool> s_ps5VibrationSuccessLogged{ false };
static bool s_ps5PadOpenErrorLogged;
static XAMINPUT_VIBRATION s_ps5GameVibration{};
static bool s_ps5MenuVibrationActive;
static bool s_ps5MenuVibrationStrengthOverrideActive;
static float s_ps5MenuVibrationStrengthOverride = 1.0f;
static std::chrono::steady_clock::time_point s_ps5MenuVibrationEnd;

static bool EnsurePs5CompatibleVibrationModeLocked()
{
    if (s_ps5PadHandle < 0)
        return false;
    if (s_ps5HapticAudioActive.load(std::memory_order_acquire))
        return true;
    if (s_ps5CompatibleVibrationMode.load(std::memory_order_acquire))
        return true;

    // Fallback for controllers/transport modes without a usable vibration audio port.
    const int result = scePadSetVibrationMode(s_ps5PadHandle, PS5_VIBRATION_MODE_COMPATIBLE);
    const bool ready = result == 0;
    s_ps5CompatibleVibrationMode.store(ready, std::memory_order_release);
    if (!ready && !s_ps5VibrationModeErrorLogged.exchange(true, std::memory_order_relaxed))
    {
        LOGFN_ERROR("scePadSetVibrationMode(handle={}, mode=2) failed: {}", s_ps5PadHandle, result);
    }
    else if (ready)
    {
        s_ps5VibrationModeErrorLogged.store(false, std::memory_order_relaxed);
    }
    return ready;
}

static bool EnsurePs5CompatibleVibrationMode()
{
    std::lock_guard<std::mutex> lock(s_ps5PadVibrationMutex);
    return EnsurePs5CompatibleVibrationModeLocked();
}

static float Ps5HapticNoise(uint32_t& rng)
{
    rng = rng * 1664525u + 1013904223u;
    return static_cast<float>((rng >> 9) & 0xffffu) / 32768.0f - 1.0f;
}

static int16_t Ps5HapticToS16(float sample)
{
    const float clipped = std::clamp(sample, -1.0f, 1.0f);
    const float scale = clipped < 0.0f ? 32768.0f : 32767.0f;
    return static_cast<int16_t>(clipped * scale);
}

static void* Ps5HapticAudioWorker(void*)
{
    alignas(16) int16_t pcmS16[PS5_HAPTIC_FRAMES_PER_OUTPUT * 2]{};
    alignas(16) float pcmF32[PS5_HAPTIC_FRAMES_PER_OUTPUT * 2]{};
    double phaseLarge = 0.0;
    double phaseSmall = 0.0;
    float envelopeLarge = 0.0f;
    float envelopeSmall = 0.0f;
    float noiseLeft = 0.0f;
    float noiseRight = 0.0f;
    uint32_t rng = 0x9e3779b9u;
    int outputFailures = 0;

    while (s_ps5HapticThreadRunning.load(std::memory_order_relaxed))
    {
        for (uint32_t i = 0; i < PS5_HAPTIC_FRAMES_PER_OUTPUT; ++i)
        {
            const float largeTarget = static_cast<float>(
                s_ps5HapticLargeLevel.load(std::memory_order_relaxed)) / 65535.0f;
            const float smallTarget = static_cast<float>(
                s_ps5HapticSmallLevel.load(std::memory_order_relaxed)) / 65535.0f;
            envelopeLarge += (largeTarget - envelopeLarge) * 0.02f;
            envelopeSmall += (smallTarget - envelopeSmall) * 0.02f;

            const float randomLeft = Ps5HapticNoise(rng);
            const float randomRight = Ps5HapticNoise(rng);
            noiseLeft += 0.08f * (randomLeft - noiseLeft);
            noiseRight += 0.08f * (randomRight - noiseRight);

            phaseLarge += 55.0 / PS5_HAPTIC_SAMPLE_RATE;
            phaseSmall += 190.0 / PS5_HAPTIC_SAMPLE_RATE;
            if (phaseLarge >= 1.0)
                phaseLarge -= 1.0;
            if (phaseSmall >= 1.0)
                phaseSmall -= 1.0;

            constexpr double TWO_PI = 6.28318530717958647692;
            const float largeSine = static_cast<float>(std::sin(TWO_PI * phaseLarge));
            const float smallSine = static_cast<float>(std::sin(TWO_PI * phaseSmall));
            const float left = envelopeLarge * (0.70f * largeSine + 0.50f * noiseLeft) +
                               envelopeSmall * (0.55f * smallSine + 0.45f * noiseRight);
            const float right = envelopeLarge * (0.70f * largeSine + 0.50f * noiseRight) +
                                envelopeSmall * (0.55f * smallSine + 0.45f * noiseLeft);
            const float leftClipped = std::clamp(left * (26000.0f / 32768.0f), -1.0f, 1.0f);
            const float rightClipped = std::clamp(right * (26000.0f / 32768.0f), -1.0f, 1.0f);
            pcmF32[2 * i] = leftClipped;
            pcmF32[2 * i + 1] = rightClipped;
            pcmS16[2 * i] = Ps5HapticToS16(leftClipped);
            pcmS16[2 * i + 1] = Ps5HapticToS16(rightClipped);
        }

        const void* samples = s_ps5HapticFloat32Format
                                  ? static_cast<const void*>(pcmF32)
                                  : static_cast<const void*>(pcmS16);
        if (sceAudioOutOutput(s_ps5HapticPort, samples) < 0)
        {
            if (++outputFailures >= PS5_HAPTIC_OUTPUT_FAILURE_LIMIT)
            {
                int modeResult = -1;
                bool compatibleReady = false;
                {
                    // Keep mode change and the full two-motor state update
                    // atomic with respect to game-thread vibration requests.
                    std::lock_guard<std::mutex> vibrationLock(s_ps5PadVibrationMutex);
                    modeResult = scePadSetVibrationMode(s_ps5PadHandle, PS5_VIBRATION_MODE_COMPATIBLE);
                    compatibleReady = modeResult == 0;
                    s_ps5CompatibleVibrationMode.store(compatibleReady, std::memory_order_release);
                    s_ps5HapticAudioActive.store(false, std::memory_order_release);

                    if (compatibleReady)
                    {
                        const uint16_t fallbackLarge =
                            s_ps5RumbleFallbackLargeLevel.load(std::memory_order_relaxed);
                        const uint16_t fallbackSmall =
                            s_ps5RumbleFallbackSmallLevel.load(std::memory_order_relaxed);
                        s_ps5HapticLargeLevel.store(fallbackLarge, std::memory_order_relaxed);
                        s_ps5HapticSmallLevel.store(fallbackSmall, std::memory_order_relaxed);
                        const Ps5ScePadVibrationParam levels{
                            static_cast<uint8_t>(fallbackLarge >> 8),
                            static_cast<uint8_t>(fallbackSmall >> 8),
                        };
                        const int vibrationResult = scePadSetVibration(s_ps5PadHandle, &levels);
                        if (vibrationResult != 0)
                            LOGFN_ERROR("scePadSetVibration fallback after haptic stream failure failed: {}",
                                        vibrationResult);
                    }
                    else
                    {
                        LOGFN_ERROR("scePadSetVibrationMode(handle={}, mode=2) fallback failed after haptic stream error: {}",
                                    s_ps5PadHandle, modeResult);
                    }
                }

                const int32_t failedPort = s_ps5HapticPort;
                s_ps5HapticPort = -1;
                if (failedPort >= 0)
                    (void)sceAudioOutClose(failedPort);
                if (!s_ps5HapticOutputFailureLogged.exchange(true, std::memory_order_relaxed))
                {
                    LOGFN_WARNING("DualSense haptic audio output failed repeatedly on port {}; mode-2 fallback result={}",
                                  failedPort, modeResult);
                }
                s_ps5HapticThreadRunning.store(false, std::memory_order_release);
                return nullptr;
            }
            (void)usleep(2000);
        }
        else
        {
            outputFailures = 0;
        }
    }
    return nullptr;
}

static bool TryStartPs5HapticAudio()
{
    std::lock_guard<std::mutex> lock(s_ps5HapticInitMutex);
    if (s_ps5PadHandle < 0 || s_ps5PadUserId < 0)
        return false;
    if (s_ps5HapticAudioActive.load(std::memory_order_acquire))
        return true;
    if (s_ps5HapticThreadRunning.load(std::memory_order_acquire) || s_ps5HapticThreadStartFailed)
        return false;

    const auto now = std::chrono::steady_clock::now();
    if (s_ps5HapticAttemptTimeSet && now - s_ps5LastHapticPortAttempt < std::chrono::seconds(2))
        return false;
    s_ps5LastHapticPortAttempt = now;
    s_ps5HapticAttemptTimeSet = true;

    const int initResult = sceAudioOutInit();
    if (initResult != 0 && static_cast<uint32_t>(initResult) != PS5_AUDIO_OUT_ALREADY_INITIALIZED)
    {
        if (!s_ps5HapticAudioInitErrorLogged)
        {
            LOGFN_WARNING("sceAudioOutInit for DualSense haptics returned {}; keeping compatible motor rumble",
                          initResult);
            s_ps5HapticAudioInitErrorLogged = true;
        }
        return false;
    }
    s_ps5HapticAudioInitErrorLogged = false;

    int32_t port = sceAudioOutOpen(s_ps5PadUserId, PS5_AUDIO_PORT_TYPE_VIBRATION, 0,
                                   PS5_HAPTIC_FRAMES_PER_OUTPUT, PS5_HAPTIC_SAMPLE_RATE,
                                   PS5_HAPTIC_FORMAT_S16_STEREO);
    bool float32Format = false;
    const int32_t s16OpenResult = port;
    int32_t f32OpenResult = -1;
    if (port < 0)
    {
        f32OpenResult = sceAudioOutOpen(s_ps5PadUserId, PS5_AUDIO_PORT_TYPE_VIBRATION, 0,
                                        PS5_HAPTIC_FRAMES_PER_OUTPUT, PS5_HAPTIC_SAMPLE_RATE,
                                        PS5_HAPTIC_FORMAT_F32_STEREO);
        port = f32OpenResult;
        float32Format = port >= 0;
    }
    if (port < 0)
    {
        if (!s_ps5HapticPortOpenFailureLogged)
        {
            LOGFN_WARNING("DualSense vibration audio port type=10 unavailable (S16 result={}, F32 result={}); using compatible motor rumble",
                          s16OpenResult, f32OpenResult);
            s_ps5HapticPortOpenFailureLogged = true;
        }
        return false;
    }

    int modeResult = -1;
    {
        std::lock_guard<std::mutex> vibrationLock(s_ps5PadVibrationMutex);
        modeResult = scePadSetVibrationMode(s_ps5PadHandle, PS5_VIBRATION_MODE_ADVANCED);
        if (modeResult == 0)
        {
            s_ps5HapticPort = port;
            s_ps5HapticFloat32Format = float32Format;
            s_ps5CompatibleVibrationMode.store(false, std::memory_order_release);
            // Publish the advanced route while holding the same lock used by
            // motor requests, so none can switch the pad back to mode 2.
            s_ps5HapticAudioActive.store(true, std::memory_order_release);
            s_ps5HapticLargeLevel.store(
                s_ps5HapticRouteLargeLevel.load(std::memory_order_relaxed), std::memory_order_relaxed);
            s_ps5HapticSmallLevel.store(
                s_ps5HapticRouteSmallLevel.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        else
        {
            s_ps5CompatibleVibrationMode.store(false, std::memory_order_release);
        }
    }
    if (modeResult != 0)
    {
        (void)sceAudioOutClose(port);
        (void)EnsurePs5CompatibleVibrationMode();
        if (!s_ps5HapticModeErrorLogged)
        {
            LOGFN_WARNING("scePadSetVibrationMode(handle={}, mode=1) failed: {}; keeping compatible motor rumble",
                          s_ps5PadHandle, modeResult);
            s_ps5HapticModeErrorLogged = true;
        }
        return false;
    }

    s_ps5HapticThreadRunning.store(true, std::memory_order_release);
    const int threadResult = pthread_create(&s_ps5HapticThread, nullptr, Ps5HapticAudioWorker, nullptr);
    if (threadResult != 0)
    {
        s_ps5HapticThreadRunning.store(false, std::memory_order_release);
        int fallbackResult = -1;
        bool fallbackReady = false;
        {
            std::lock_guard<std::mutex> vibrationLock(s_ps5PadVibrationMutex);
            fallbackResult = scePadSetVibrationMode(s_ps5PadHandle, PS5_VIBRATION_MODE_COMPATIBLE);
            fallbackReady = fallbackResult == 0;
            s_ps5CompatibleVibrationMode.store(fallbackReady, std::memory_order_release);
            s_ps5HapticAudioActive.store(false, std::memory_order_release);
            if (fallbackReady)
            {
                const uint16_t fallbackLarge =
                    s_ps5RumbleFallbackLargeLevel.load(std::memory_order_relaxed);
                const uint16_t fallbackSmall =
                    s_ps5RumbleFallbackSmallLevel.load(std::memory_order_relaxed);
                s_ps5HapticLargeLevel.store(fallbackLarge, std::memory_order_relaxed);
                s_ps5HapticSmallLevel.store(fallbackSmall, std::memory_order_relaxed);
                const Ps5ScePadVibrationParam levels{
                    static_cast<uint8_t>(fallbackLarge >> 8),
                    static_cast<uint8_t>(fallbackSmall >> 8),
                };
                const int vibrationResult = scePadSetVibration(s_ps5PadHandle, &levels);
                if (vibrationResult != 0)
                    LOGFN_ERROR("scePadSetVibration after haptic worker start failure failed: {}", vibrationResult);
            }
        }
        if (!fallbackReady)
        {
            LOGFN_ERROR("scePadSetVibrationMode(handle={}, mode=2) fallback after worker start failure failed: {}",
                        s_ps5PadHandle, fallbackResult);
        }
        const int32_t failedPort = s_ps5HapticPort;
        s_ps5HapticPort = -1;
        if (failedPort >= 0)
            (void)sceAudioOutClose(failedPort);
        s_ps5HapticThreadStartFailed = true;
        LOGFN_WARNING("Could not start DualSense haptic worker thread: {}; using compatible motor rumble",
                      threadResult);
        return false;
    }
    (void)pthread_detach(s_ps5HapticThread);

    s_ps5HapticPortOpenFailureLogged = false;
    s_ps5HapticModeErrorLogged = false;
    s_ps5HapticOutputFailureLogged.store(false, std::memory_order_relaxed);
    LOGFN("DualSense haptic audio active (port={}, user={}, format={}, 48 kHz/256 frames, vibration mode=1)",
          s_ps5HapticPort, s_ps5PadUserId, float32Format ? "F32 stereo" : "S16 stereo");
    return true;
}

static void SetPs5PadVibration(
    const XAMINPUT_VIBRATION& rumbleOutput,
    const XAMINPUT_VIBRATION& hapticOutput)
{
    std::lock_guard<std::mutex> vibrationLock(s_ps5PadVibrationMutex);

    s_ps5RumbleFallbackLargeLevel.store(rumbleOutput.wLeftMotorSpeed, std::memory_order_relaxed);
    s_ps5RumbleFallbackSmallLevel.store(rumbleOutput.wRightMotorSpeed, std::memory_order_relaxed);
    s_ps5HapticRouteLargeLevel.store(hapticOutput.wLeftMotorSpeed, std::memory_order_relaxed);
    s_ps5HapticRouteSmallLevel.store(hapticOutput.wRightMotorSpeed, std::memory_order_relaxed);

    const bool hapticAudioActive = s_ps5HapticAudioActive.load(std::memory_order_acquire);
    const XAMINPUT_VIBRATION output = hapticAudioActive ? hapticOutput : rumbleOutput;
    const uint16_t largeLevel = output.wLeftMotorSpeed;
    const uint16_t smallLevel = output.wRightMotorSpeed;
    s_ps5HapticLargeLevel.store(largeLevel, std::memory_order_relaxed);
    s_ps5HapticSmallLevel.store(smallLevel, std::memory_order_relaxed);

    if (hapticAudioActive)
    {
        if ((largeLevel != 0 || smallLevel != 0) &&
            !s_ps5HapticRequestLogged.exchange(true, std::memory_order_relaxed))
        {
            LOGFN("Nonzero DualSense vibration request routed through PCM haptics (large={}, small={})",
                  largeLevel, smallLevel);
        }
        return;
    }
    if (!EnsurePs5CompatibleVibrationModeLocked())
        return;

    // XInput left is low-frequency/strong rumble (large motor); right is
    // high-frequency/weak rumble (small motor). scePad expects 8-bit levels.
    const Ps5ScePadVibrationParam levels{
        static_cast<uint8_t>(largeLevel >> 8),
        static_cast<uint8_t>(smallLevel >> 8),
    };
    const int result = scePadSetVibration(s_ps5PadHandle, &levels);
    if (result != 0)
    {
        if (!s_ps5VibrationErrorLogged.exchange(true, std::memory_order_relaxed))
            LOGFN_ERROR("scePadSetVibration(handle={}) failed: {}", s_ps5PadHandle, result);
    }
    else
    {
        s_ps5VibrationErrorLogged.store(false, std::memory_order_relaxed);
        if ((levels.largeMotor != 0 || levels.smallMotor != 0) &&
            !s_ps5VibrationSuccessLogged.exchange(true, std::memory_order_relaxed))
        {
            LOGFN("scePadSetVibration accepted a request (handle={}, large={}, small={})",
                  s_ps5PadHandle, levels.largeMotor, levels.smallMotor);
        }
    }
}

static XAMINPUT_VIBRATION MaxVibrationLevels(
    const XAMINPUT_VIBRATION& first,
    const XAMINPUT_VIBRATION& second)
{
    return {
        std::max(first.wLeftMotorSpeed, second.wLeftMotorSpeed),
        std::max(first.wRightMotorSpeed, second.wRightMotorSpeed)
    };
}

static void ApplyPs5PadVibration()
{
    XAMINPUT_VIBRATION menuPulse{ 0, 0 };
    float menuRumbleMultiplier = Config::RumbleStrength.Value;
    float menuHapticMultiplier = Config::VibrationStrength.Value;
    if (s_ps5MenuVibrationActive)
    {
        if (std::chrono::steady_clock::now() < s_ps5MenuVibrationEnd)
        {
            menuPulse = MENU_VIBRATION_PULSE;
            if (s_ps5MenuVibrationStrengthOverrideActive)
            {
                menuRumbleMultiplier = s_ps5MenuVibrationStrengthOverride;
                menuHapticMultiplier = s_ps5MenuVibrationStrengthOverride;
            }
        }
        else
        {
            s_ps5MenuVibrationActive = false;
            s_ps5MenuVibrationStrengthOverrideActive = false;
        }
    }

    // PS5 game rumble uses the selected route slider directly. Boost doubles
    // that configured gain, capped at full scale; menu pulses are not doubled.
    const float boostMultiplier = hid::IsBoostRumbleActive()
        ? PS5_BOOST_RUMBLE_MULTIPLIER
        : 1.0f;
    const XAMINPUT_VIBRATION rumbleGame = ScaleVibrationByMultiplier(
        s_ps5GameVibration, Config::RumbleStrength.Value * boostMultiplier);
    const XAMINPUT_VIBRATION rumbleMenu =
        ScaleVibrationByMultiplier(menuPulse, menuRumbleMultiplier);
    const XAMINPUT_VIBRATION rumbleOutput = MaxVibrationLevels(rumbleGame, rumbleMenu);

    const XAMINPUT_VIBRATION hapticGame = ScaleVibrationByMultiplier(
        s_ps5GameVibration, Config::VibrationStrength.Value * boostMultiplier);
    const XAMINPUT_VIBRATION hapticMenu =
        ScaleVibrationByMultiplier(menuPulse, menuHapticMultiplier);
    const XAMINPUT_VIBRATION hapticOutput = MaxVibrationLevels(hapticGame, hapticMenu);

    SetPs5PadVibration(rumbleOutput, hapticOutput);
}

static void UpdatePs5MenuVibration()
{
    if (s_ps5MenuVibrationActive && std::chrono::steady_clock::now() >= s_ps5MenuVibrationEnd)
        ApplyPs5PadVibration();
}

static void InitPs5Pad()
{
    if (s_ps5PadHandle >= 0)
        return;

    const int userServiceInitResult = sceUserServiceInitialize(nullptr);
    const int padInitResult = scePadInit();

    int32_t userId = -1;
    const int initialUserResult = sceUserServiceGetInitialUser(&userId);
    if (initialUserResult != 0 || userId < 0)
        userId = 1;

    int32_t padUserId = userId;
    s_ps5PadHandle = scePadOpen(padUserId, 0, 0, nullptr);
    if (s_ps5PadHandle < 0)
        s_ps5PadHandle = scePadGetHandle(padUserId, 0, 0);
    if (s_ps5PadHandle >= 0)
    {
        s_ps5PadUserId = padUserId;
    }
    else
    {
        static const int32_t kFallbackIds[] = { 1, 0, 0x10000000, 0xFF };
        for (int32_t uid : kFallbackIds)
        {
            int32_t candidateHandle = scePadOpen(uid, 0, 0, nullptr);
            if (candidateHandle < 0)
                candidateHandle = scePadGetHandle(uid, 0, 0);
            if (candidateHandle >= 0)
            {
                s_ps5PadHandle = candidateHandle;
                s_ps5PadUserId = uid;
                break;
            }
        }
    }

    if (s_ps5PadHandle >= 0)
    {
        s_ps5PadOpenErrorLogged = false;
        hid::g_inputDevice = hid::EInputDevice::PlayStation;
        hid::g_inputDeviceController = hid::EInputDevice::PlayStation;
        hid::g_inputDeviceExplicit = hid::EInputDeviceExplicit::DualSense;
        const bool compatibleFallbackReady = EnsurePs5CompatibleVibrationMode();
        const bool hapticAudioActive = TryStartPs5HapticAudio();
        LOGFN("Opened PS5 DualSense via scePad (handle={}, user={}, vibration mode={}, audio haptics active={}, compatible fallback ready={})",
              s_ps5PadHandle, s_ps5PadUserId, hapticAudioActive ? PS5_VIBRATION_MODE_ADVANCED : PS5_VIBRATION_MODE_COMPATIBLE,
              hapticAudioActive, compatibleFallbackReady);
    }
    else if (!s_ps5PadOpenErrorLogged)
    {
        LOGFN_ERROR("Could not open a PS5 pad (user-service init={}, pad init={}, initial-user result={}, fallback user={})",
                    userServiceInitResult, padInitResult, initialUserResult, userId);
        s_ps5PadOpenErrorLogged = true;
    }
}

static bool PollPs5Pad(XAMINPUT_GAMEPAD& pad)
{
    if (s_ps5PadHandle < 0)
        InitPs5Pad();
    if (s_ps5PadHandle < 0)
        return false;

    Ps5ScePadData data{};
    if (scePadReadState(s_ps5PadHandle, &data) != 0)
        return false;

    // A controller may be plugged in after startup. Retry the optional audio
    // haptic port at a throttled interval while compatible motor rumble works.
    if (!s_ps5HapticAudioActive.load(std::memory_order_acquire) &&
        !s_ps5HapticThreadRunning.load(std::memory_order_acquire))
    {
        (void)TryStartPs5HapticAudio();
    }

    pad.wButtons = 0;
    if (data.buttons & 0x00000010u) pad.wButtons |= XAMINPUT_GAMEPAD_DPAD_UP;
    if (data.buttons & 0x00000040u) pad.wButtons |= XAMINPUT_GAMEPAD_DPAD_DOWN;
    if (data.buttons & 0x00000080u) pad.wButtons |= XAMINPUT_GAMEPAD_DPAD_LEFT;
    if (data.buttons & 0x00000020u) pad.wButtons |= XAMINPUT_GAMEPAD_DPAD_RIGHT;
    if (data.buttons & 0x00000008u) pad.wButtons |= XAMINPUT_GAMEPAD_START;
    if (data.buttons & 0x00100000u) pad.wButtons |= XAMINPUT_GAMEPAD_BACK;
    if (data.buttons & 0x00000002u) pad.wButtons |= XAMINPUT_GAMEPAD_LEFT_THUMB;
    if (data.buttons & 0x00000004u) pad.wButtons |= XAMINPUT_GAMEPAD_RIGHT_THUMB;
    if (data.buttons & 0x00000400u) pad.wButtons |= XAMINPUT_GAMEPAD_LEFT_SHOULDER;
    if (data.buttons & 0x00000800u) pad.wButtons |= XAMINPUT_GAMEPAD_RIGHT_SHOULDER;
    if (data.buttons & 0x00004000u) pad.wButtons |= XAMINPUT_GAMEPAD_A;
    if (data.buttons & 0x00002000u) pad.wButtons |= XAMINPUT_GAMEPAD_B;
    if (data.buttons & 0x00008000u) pad.wButtons |= XAMINPUT_GAMEPAD_X;
    if (data.buttons & 0x00001000u) pad.wButtons |= XAMINPUT_GAMEPAD_Y;

    auto scaleAxis = [](uint8_t raw, bool invert) -> int16_t {
        int32_t centered = (int32_t)raw - 128;
        if (centered >= -10 && centered <= 10)
            return 0;
        if (invert)
            centered = -centered;
        int32_t scaled = centered * 256;
        if (scaled > 32767) scaled = 32767;
        if (scaled < -32768) scaled = -32768;
        return (int16_t)scaled;
    };

    pad.sThumbLX = scaleAxis(data.leftStickX, false);
    pad.sThumbLY = scaleAxis(data.leftStickY, true);
    pad.sThumbRX = scaleAxis(data.rightStickX, false);
    pad.sThumbRY = scaleAxis(data.rightStickY, true);
    pad.bLeftTrigger = data.analogL2 ? data.analogL2 : ((data.buttons & 0x00000100u) ? 255 : 0);
    pad.bRightTrigger = data.analogR2 ? data.analogR2 : ((data.buttons & 0x00000200u) ? 255 : 0);

    hid::g_inputDevice = hid::EInputDevice::PlayStation;
    hid::g_inputDeviceController = hid::EInputDevice::PlayStation;
    hid::g_inputDeviceExplicit = hid::EInputDeviceExplicit::DualSense;
    return true;
}

void PumpPs5PadEvents()
{
    UpdatePs5MenuVibration();

    static uint16_t s_prevButtons = 0;
    static int16_t s_prevLX = 0, s_prevLY = 0, s_prevRX = 0, s_prevRY = 0;
    XAMINPUT_GAMEPAD pad{};
    if (!PollPs5Pad(pad))
        return;

    struct BtnMap { uint16_t mask; SDL_GameControllerButton sdlBtn; };
    static const BtnMap kMap[] = {
        { XAMINPUT_GAMEPAD_DPAD_UP,        SDL_CONTROLLER_BUTTON_DPAD_UP },
        { XAMINPUT_GAMEPAD_DPAD_DOWN,      SDL_CONTROLLER_BUTTON_DPAD_DOWN },
        { XAMINPUT_GAMEPAD_DPAD_LEFT,      SDL_CONTROLLER_BUTTON_DPAD_LEFT },
        { XAMINPUT_GAMEPAD_DPAD_RIGHT,     SDL_CONTROLLER_BUTTON_DPAD_RIGHT },
        { XAMINPUT_GAMEPAD_START,          SDL_CONTROLLER_BUTTON_START },
        { XAMINPUT_GAMEPAD_BACK,           SDL_CONTROLLER_BUTTON_BACK },
        { XAMINPUT_GAMEPAD_LEFT_THUMB,     SDL_CONTROLLER_BUTTON_LEFTSTICK },
        { XAMINPUT_GAMEPAD_RIGHT_THUMB,    SDL_CONTROLLER_BUTTON_RIGHTSTICK },
        { XAMINPUT_GAMEPAD_LEFT_SHOULDER,  SDL_CONTROLLER_BUTTON_LEFTSHOULDER },
        { XAMINPUT_GAMEPAD_RIGHT_SHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER },
        { XAMINPUT_GAMEPAD_A,              SDL_CONTROLLER_BUTTON_A },
        { XAMINPUT_GAMEPAD_B,              SDL_CONTROLLER_BUTTON_B },
        { XAMINPUT_GAMEPAD_X,              SDL_CONTROLLER_BUTTON_X },
        { XAMINPUT_GAMEPAD_Y,              SDL_CONTROLLER_BUTTON_Y },
    };

    uint16_t diff = pad.wButtons ^ s_prevButtons;
    if (diff)
    {
        for (const auto& m : kMap)
        {
            if (diff & m.mask)
            {
                bool pressed = (pad.wButtons & m.mask) != 0;
                SDL_Event ev{};
                ev.type = pressed ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
                ev.cbutton.which = 0;
                ev.cbutton.button = (Uint8)m.sdlBtn;
                ev.cbutton.state = pressed ? SDL_PRESSED : SDL_RELEASED;
                SDL_PushEvent(&ev);
            }
        }
        s_prevButtons = pad.wButtons;
    }

    auto pushAxis = [](SDL_GameControllerAxis axis, int16_t val, int16_t& prev) {
        if (std::abs((int)val - (int)prev) > 2048 || (val == 0 && prev != 0))
        {
            prev = val;
            SDL_Event ev{};
            ev.type = SDL_CONTROLLERAXISMOTION;
            ev.caxis.which = 0;
            ev.caxis.axis = (Uint8)axis;
            ev.caxis.value = val;
            SDL_PushEvent(&ev);
        }
    };
    pushAxis(SDL_CONTROLLER_AXIS_LEFTX,  pad.sThumbLX,  s_prevLX);
    pushAxis(SDL_CONTROLLER_AXIS_LEFTY,  ~pad.sThumbLY, s_prevLY);
    pushAxis(SDL_CONTROLLER_AXIS_RIGHTX, pad.sThumbRX,  s_prevRX);
    pushAxis(SDL_CONTROLLER_AXIS_RIGHTY, ~pad.sThumbRY, s_prevRY);
}
#endif

#define TRANSLATE_INPUT(S, X) SDL_GameControllerGetButton(controller, S) << FirstBitLow(X)
#define VIBRATION_TIMEOUT_MS 5000

class Controller
{
public:
    SDL_GameController* controller{};
    SDL_Joystick* joystick{};
    SDL_JoystickID id{ -1 };
    XAMINPUT_GAMEPAD state{};
    XAMINPUT_VIBRATION vibration{ 0, 0 };
    int index{};

    Controller() = default;

    explicit Controller(int index) : Controller(SDL_GameControllerOpen(index))
    {
        this->index = index;
    }

    Controller(SDL_GameController* controller) : controller(controller)
    {
        if (!controller)
            return;

        joystick = SDL_GameControllerGetJoystick(controller);
        id = SDL_JoystickInstanceID(joystick);
    }

    SDL_GameControllerType GetControllerType() const
    {
        return SDL_GameControllerGetType(controller);
    }

    hid::EInputDevice GetInputDevice() const
    {
        switch (GetControllerType())
        {
            case SDL_CONTROLLER_TYPE_PS3:
            case SDL_CONTROLLER_TYPE_PS4:
            case SDL_CONTROLLER_TYPE_PS5:
                return hid::EInputDevice::PlayStation;
            case SDL_CONTROLLER_TYPE_XBOX360:
            case SDL_CONTROLLER_TYPE_XBOXONE:
                return hid::EInputDevice::Xbox;
            default:
                return hid::EInputDevice::Unknown;
        }
    }

    const char* GetControllerName() const
    {
        auto result = SDL_GameControllerName(controller);

        if (!result)
            return "Unknown Device";

        return result;
    }

    void Close()
    {
        if (!controller)
            return;

        SDL_GameControllerClose(controller);

        controller = nullptr;
        joystick = nullptr;
        id = -1;
    }

    bool CanPoll()
    {
        return controller;
    }

    void PollAxis()
    {
        if (!CanPoll())
            return;

        auto& pad = state;

        pad.sThumbLX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        pad.sThumbLY = ~SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

        pad.sThumbRX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        pad.sThumbRY = ~SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

        pad.bLeftTrigger = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7;
        pad.bRightTrigger = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7;
    }

    void Poll()
    {
        if (!CanPoll())
            return;

        auto& pad = state;

        pad.wButtons = 0;

        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_DPAD_UP, XAMINPUT_GAMEPAD_DPAD_UP);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_DPAD_DOWN, XAMINPUT_GAMEPAD_DPAD_DOWN);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_DPAD_LEFT, XAMINPUT_GAMEPAD_DPAD_LEFT);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, XAMINPUT_GAMEPAD_DPAD_RIGHT);

        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_START, XAMINPUT_GAMEPAD_START);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_BACK, XAMINPUT_GAMEPAD_BACK);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_TOUCHPAD, XAMINPUT_GAMEPAD_BACK);

        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_LEFTSTICK, XAMINPUT_GAMEPAD_LEFT_THUMB);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_RIGHTSTICK, XAMINPUT_GAMEPAD_RIGHT_THUMB);

        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, XAMINPUT_GAMEPAD_LEFT_SHOULDER);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, XAMINPUT_GAMEPAD_RIGHT_SHOULDER);

        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_A, XAMINPUT_GAMEPAD_A);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_B, XAMINPUT_GAMEPAD_B);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_X, XAMINPUT_GAMEPAD_X);
        pad.wButtons |= TRANSLATE_INPUT(SDL_CONTROLLER_BUTTON_Y, XAMINPUT_GAMEPAD_Y);
    }

    void SetVibration(const XAMINPUT_VIBRATION& vibration)
    {
        if (!CanPoll())
            return;

        this->vibration = vibration;
        const XAMINPUT_VIBRATION scaledVibration =
            ScaleVibrationByMultiplier(vibration, Config::RumbleStrength.Value);

        SDL_GameControllerRumble(controller, scaledVibration.wLeftMotorSpeed,
            scaledVibration.wRightMotorSpeed, VIBRATION_TIMEOUT_MS);
    }

    void SetLED(const uint8_t r, const uint8_t g, const uint8_t b) const
    {
        SDL_GameControllerSetLED(controller, r, g, b);
    }
};

std::array<Controller, 4> g_controllers;
Controller* g_activeController;

static std::chrono::steady_clock::time_point s_lastMenuVibration;
static bool s_hasMenuVibrationTimestamp;
#if defined(__PROSPERO__)
static bool s_menuVibrationDisabledLogged;
static bool s_menuVibrationRouteLogged;
static bool s_menuVibrationUnavailableLogged;
#endif

void hid::RefreshVibrationOutput()
{
#if defined(__PROSPERO__)
    // Recompute both the active route and the atomically cached motor fallback
    // from the latest route strengths. The audio worker never reads Config data.
    if (s_ps5PadHandle >= 0)
        ApplyPs5PadVibration();
#endif
}

static void SendMenuVibrationPulse(bool useStrengthOverride, float strengthOverride)
{
    if (!Config::Vibration.Value || !Config::VibrationMenu.Value)
    {
#if defined(__PROSPERO__)
        if (!s_menuVibrationDisabledLogged)
        {
            LOGN("Menu vibration was requested but is disabled in the Input settings");
            s_menuVibrationDisabledLogged = true;
        }
#endif
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool isOnCooldown = s_hasMenuVibrationTimestamp &&
        now - s_lastMenuVibration < std::chrono::milliseconds(55);
#if defined(__PROSPERO__)
    if (isOnCooldown)
    {
        // Let a slider preview track the latest value without extending the pulse.
        if (useStrengthOverride && s_ps5PadHandle >= 0 && s_ps5MenuVibrationActive)
        {
            s_ps5MenuVibrationStrengthOverrideActive = true;
            s_ps5MenuVibrationStrengthOverride = strengthOverride;
            ApplyPs5PadVibration();
        }
        return;
    }
#else
    if (isOnCooldown)
        return;
#endif

    s_lastMenuVibration = now;
    s_hasMenuVibrationTimestamp = true;

    // A brief dual-motor pulse, also used to preview slider force.
    constexpr auto pulseDuration = std::chrono::milliseconds(MENU_VIBRATION_DURATION_MS);
#if defined(__PROSPERO__)
    // Prefer the native scePad handle even if SDL classified the PS5 remote or
    // a virtual controller as Unknown; SDL's active device is not the rumble target.
    if (s_ps5PadHandle < 0)
        InitPs5Pad();
    if (s_ps5PadHandle >= 0)
    {
        if (!s_menuVibrationRouteLogged)
        {
            LOGFN("Menu vibration pulse reached native scePad route (handle={})", s_ps5PadHandle);
            s_menuVibrationRouteLogged = true;
        }
        s_ps5MenuVibrationActive = true;
        s_ps5MenuVibrationEnd = now + pulseDuration;
        s_ps5MenuVibrationStrengthOverrideActive = useStrengthOverride;
        s_ps5MenuVibrationStrengthOverride = strengthOverride;
        ApplyPs5PadVibration();
        return;
    }
#endif

    if (g_activeController && g_activeController->controller)
    {
        const float multiplier = useStrengthOverride
            ? strengthOverride
            : Config::RumbleStrength.Value;
        const XAMINPUT_VIBRATION scaledMenuPulse =
            ScaleVibrationByMultiplier(MENU_VIBRATION_PULSE, multiplier);
#if defined(__PROSPERO__)
        const int result = SDL_GameControllerRumble(g_activeController->controller,
            scaledMenuPulse.wLeftMotorSpeed, scaledMenuPulse.wRightMotorSpeed,
            static_cast<Uint32>(pulseDuration.count()));
        if (!s_menuVibrationRouteLogged)
        {
            LOGFN("Menu vibration pulse routed through SDL controller '{}' (result={})",
                g_activeController->GetControllerName(), result);
            s_menuVibrationRouteLogged = true;
        }
#else
        SDL_GameControllerRumble(g_activeController->controller,
            scaledMenuPulse.wLeftMotorSpeed, scaledMenuPulse.wRightMotorSpeed,
            static_cast<Uint32>(pulseDuration.count()));
#endif
    }
#if defined(__PROSPERO__)
    else if (!s_menuVibrationUnavailableLogged)
    {
        LOGN_ERROR("Menu vibration pulse requested but neither scePad nor SDL has an open controller");
        s_menuVibrationUnavailableLogged = true;
    }
#endif
}

void hid::PulseMenuVibration()
{
    SendMenuVibrationPulse(false, 1.0f);
}

void hid::PreviewVibrationStrength(float multiplier)
{
    SendMenuVibrationPulse(true, multiplier);
}

inline Controller* EnsureController(uint32_t dwUserIndex)
{
    if (!g_controllers[dwUserIndex].controller)
        return nullptr;

    return &g_controllers[dwUserIndex];
}

inline size_t FindFreeController()
{
    for (size_t i = 0; i < g_controllers.size(); i++)
    {
        if (!g_controllers[i].controller)
            return i;
    }

    return -1;
}

inline Controller* FindController(int which)
{
    for (auto& controller : g_controllers)
    {
        if (controller.id == which)
            return &controller;
    }

    return nullptr;
}

static void SetControllerInputDevice(Controller* controller)
{
    g_activeController = controller;

    if (App::s_isLoading)
        return;

    hid::g_inputDevice = controller->GetInputDevice();
    hid::g_inputDeviceController = hid::g_inputDevice;

    auto controllerType = (hid::EInputDeviceExplicit)controller->GetControllerType();
    auto controllerName = controller->GetControllerName();

    // Only proceed if the controller type changes.
    if (hid::g_inputDeviceExplicit != controllerType)
    {
        hid::g_inputDeviceExplicit = controllerType;

        if (controllerType == hid::EInputDeviceExplicit::Unknown)
        {
            LOGFN("Detected controller: {} (Unknown Controller Type)", controllerName);
        }
        else
        {
            LOGFN("Detected controller: {}", controllerName);
        }
    }
}

static void SetControllerTimeOfDayLED(Controller& controller, bool isNight)
{
    auto r = isNight ? 22 : 0;
    auto g = isNight ? 0 : 37;
    auto b = isNight ? 101 : 184;

    controller.SetLED(r, g, b);
}

int HID_OnSDLEvent(void*, SDL_Event* event)
{
    switch (event->type)
    {
        case SDL_CONTROLLERDEVICEADDED:
        {
            const auto freeIndex = FindFreeController();

            if (freeIndex != -1)
            {
                auto controller = Controller(event->cdevice.which);

                g_controllers[freeIndex] = controller;

                SetControllerTimeOfDayLED(controller, App::s_isWerehog);
            }

            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED:
        {
            auto* controller = FindController(event->cdevice.which);

            if (controller)
                controller->Close();

            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERTOUCHPADDOWN:
        {
            auto* controller = FindController(event->cdevice.which);

            if (!controller)
                break;

            if (event->type == SDL_CONTROLLERAXISMOTION)
            {
                if (abs(event->caxis.value) > 8000)
                {
                    SDL_ShowCursor(SDL_DISABLE);
                    SetControllerInputDevice(controller);
                }

                controller->PollAxis();
            }
            else
            {
                SDL_ShowCursor(SDL_DISABLE);
                SetControllerInputDevice(controller);

                controller->Poll();
            }

            break;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            hid::g_inputDevice = hid::EInputDevice::Keyboard;
            break;

        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            if (!GameWindow::IsFullscreen() || GameWindow::s_isFullscreenCursorVisible)
                SDL_ShowCursor(SDL_ENABLE);

            hid::g_inputDevice = hid::EInputDevice::Mouse;

            break;
        }

        case SDL_WINDOWEVENT:
        {
            if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            {
                // Stop vibrating controllers on focus lost.
                for (auto& controller : g_controllers)
                    controller.SetVibration({ 0, 0 });
            }

            break;
        }

        case SDL_USER_EVILSONIC:
        {
            for (auto& controller : g_controllers)
                SetControllerTimeOfDayLED(controller, event->user.code);

            break;
        }
    }

    return 0;
}

void hid::Init()
{
#if defined(__PROSPERO__)
    InitPs5Pad();
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_WII, "1");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");
    
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0"); // Uses Button Labels. This hint is disabled for Nintendo Controllers.

    SDL_InitSubSystem(SDL_INIT_EVENTS);
    SDL_AddEventWatch(HID_OnSDLEvent, nullptr);

    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
}

uint32_t hid::GetState(uint32_t dwUserIndex, XAMINPUT_STATE* pState)
{
    static uint32_t packet;

    if (!pState)
        return ERROR_BAD_ARGUMENTS;

    memset(pState, 0, sizeof(*pState));

    pState->dwPacketNumber = packet++;

#if defined(__PROSPERO__)
    if (PollPs5Pad(pState->Gamepad))
        return ERROR_SUCCESS;
#endif

    if (!g_activeController)
        return ERROR_DEVICE_NOT_CONNECTED;

    pState->Gamepad = g_activeController->state;

    return ERROR_SUCCESS;
}

uint32_t hid::SetState(uint32_t dwUserIndex, XAMINPUT_VIBRATION* pVibration)
{
    if (!pVibration)
        return ERROR_BAD_ARGUMENTS;

#if defined(__PROSPERO__)
    if (s_ps5PadHandle >= 0 &&
        (!g_activeController || hid::g_inputDeviceController == hid::EInputDevice::PlayStation))
    {
        s_ps5GameVibration = *pVibration;
        ApplyPs5PadVibration();
        return ERROR_SUCCESS;
    }
#endif

    if (!g_activeController)
        return ERROR_DEVICE_NOT_CONNECTED;

    g_activeController->SetVibration(*pVibration);

    return ERROR_SUCCESS;
}

uint32_t hid::GetCapabilities(uint32_t dwUserIndex, XAMINPUT_CAPABILITIES* pCaps)
{
    if (!pCaps)
        return ERROR_BAD_ARGUMENTS;

#if defined(__PROSPERO__)
    if (!g_activeController && s_ps5PadHandle >= 0)
    {
        memset(pCaps, 0, sizeof(*pCaps));
        pCaps->Type = XAMINPUT_DEVTYPE_GAMEPAD;
        pCaps->SubType = XAMINPUT_DEVSUBTYPE_GAMEPAD;
        pCaps->Flags = 0;
        PollPs5Pad(pCaps->Gamepad);
        return ERROR_SUCCESS;
    }
#endif

    if (!g_activeController)
        return ERROR_DEVICE_NOT_CONNECTED;

    memset(pCaps, 0, sizeof(*pCaps));

    pCaps->Type = XAMINPUT_DEVTYPE_GAMEPAD;
    pCaps->SubType = XAMINPUT_DEVSUBTYPE_GAMEPAD; // TODO: other types?
    pCaps->Flags = 0;
    pCaps->Gamepad = g_activeController->state;
    pCaps->Vibration = g_activeController->vibration;

    return ERROR_SUCCESS;
}
