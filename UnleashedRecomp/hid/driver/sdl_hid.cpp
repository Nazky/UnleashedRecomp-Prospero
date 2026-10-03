#include <stdafx.h>
#include <SDL.h>
#include <user/config.h>
#include <hid/hid.h>
#include <os/logger.h>
#include <ui/game_window.h>
#include <kernel/xdm.h>
#include <app.h>

#if defined(__PROSPERO__)
extern "C"
{
    int sceUserServiceInitialize(const void* params);
    int sceUserServiceGetInitialUser(int32_t* userId);
    int scePadInit(void);
    int scePadOpen(int32_t userId, int32_t type, int32_t index, const void* param);
    int scePadGetHandle(int32_t userId, int32_t type, int32_t index);
    int scePadReadState(int32_t handle, void* data);
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

static int32_t s_ps5PadHandle = -1;

static void InitPs5Pad()
{
    if (s_ps5PadHandle >= 0)
        return;

    sceUserServiceInitialize(nullptr);
    scePadInit();

    int32_t userId = -1;
    if (sceUserServiceGetInitialUser(&userId) != 0 || userId < 0)
        userId = 1;

    s_ps5PadHandle = scePadOpen(userId, 0, 0, nullptr);
    if (s_ps5PadHandle < 0)
        s_ps5PadHandle = scePadGetHandle(userId, 0, 0);
    if (s_ps5PadHandle < 0)
    {
        static const int32_t kFallbackIds[] = { 1, 0, 0x10000000, 0xFF };
        for (int32_t uid : kFallbackIds)
        {
            s_ps5PadHandle = scePadOpen(uid, 0, 0, nullptr);
            if (s_ps5PadHandle < 0)
                s_ps5PadHandle = scePadGetHandle(uid, 0, 0);
            if (s_ps5PadHandle >= 0)
                break;
        }
    }

    if (s_ps5PadHandle >= 0)
    {
        hid::g_inputDevice = hid::EInputDevice::PlayStation;
        hid::g_inputDeviceController = hid::EInputDevice::PlayStation;
        hid::g_inputDeviceExplicit = hid::EInputDeviceExplicit::DualSense;
        LOGFN("Opened PS5 DualSense controller via scePad (handle={})", s_ps5PadHandle);
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

        SDL_GameControllerRumble(controller, vibration.wLeftMotorSpeed, vibration.wRightMotorSpeed, VIBRATION_TIMEOUT_MS);
    }

    void SetLED(const uint8_t r, const uint8_t g, const uint8_t b) const
    {
        SDL_GameControllerSetLED(controller, r, g, b);
    }
};

std::array<Controller, 4> g_controllers;
Controller* g_activeController;

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
    if (!g_activeController && s_ps5PadHandle >= 0)
        return ERROR_SUCCESS;
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
