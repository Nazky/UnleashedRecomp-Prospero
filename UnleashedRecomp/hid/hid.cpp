#include "hid.h"
#include <atomic>
#include <os/logger.h>
#include <ui/game_window.h>
#include <user/config.h>

hid::EInputDevice hid::g_inputDevice;
hid::EInputDevice hid::g_inputDeviceController;
hid::EInputDeviceExplicit hid::g_inputDeviceExplicit;

uint16_t hid::g_prohibitedButtons;
bool hid::g_isLeftStickProhibited;
bool hid::g_isRightStickProhibited;

static std::atomic<bool> s_boostRumbleActive{ false };

void hid::SetBoostRumbleActive(bool active)
{
    const bool wasActive = s_boostRumbleActive.exchange(active, std::memory_order_acq_rel);
#if defined(__PROSPERO__)
    if (!wasActive && active)
        LOGN("Boost vibration gain activated");
    else if (wasActive && !active)
        LOGN("Boost vibration gain deactivated");
#endif
}

bool hid::IsBoostRumbleActive()
{
    return s_boostRumbleActive.load(std::memory_order_acquire);
}

void hid::SetProhibitedInputs(uint16_t wButtons, bool leftStick, bool rightStick)
{
    hid::g_prohibitedButtons = wButtons;
    hid::g_isLeftStickProhibited = leftStick;
    hid::g_isRightStickProhibited = rightStick;
}

bool hid::IsInputAllowed()
{
#if defined(__PROSPERO__)
    return true;
#else
    return GameWindow::s_isFocused || Config::AllowBackgroundInput;
#endif
}

bool hid::IsInputDeviceController()
{
    return hid::g_inputDevice != hid::EInputDevice::Keyboard &&
        hid::g_inputDevice != hid::EInputDevice::Mouse;
}
