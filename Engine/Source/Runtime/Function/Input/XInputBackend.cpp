#include "InputBackend.h"
#include "Runtime/Function/Render/WindowManager.h"
#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")
namespace AtomEngine
{
class XInputBackend final : public InputBackend
{
  public:
    bool Initialize() override
    {
        return true;
    }
    const char *Name() const override
    {
        return "XInput + Win32";
    }
    InputAvailability Poll(InputSnapshot &s) override
    {
        InputAvailability a{true, true, false};
        for (int i = 0; i < 256; ++i)
            s.keys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(WindowManager::GetInstance()->GetWindowHandle(), &p);
        s.mouse.positionX = p.x;
        s.mouse.positionY = p.y;
        const int buttons[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        for (int i = 0; i < 5; ++i)
            if (GetAsyncKeyState(buttons[i]) & 0x8000)
                s.mouse.buttons |= 1u << i;
        XINPUT_STATE pad{};
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
            if (XInputGetState(i, &pad) == ERROR_SUCCESS)
            {
                const WORD flags[] = {XINPUT_GAMEPAD_A,
                                      XINPUT_GAMEPAD_B,
                                      XINPUT_GAMEPAD_X,
                                      XINPUT_GAMEPAD_Y,
                                      XINPUT_GAMEPAD_LEFT_SHOULDER,
                                      XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                      XINPUT_GAMEPAD_LEFT_THUMB,
                                      XINPUT_GAMEPAD_RIGHT_THUMB,
                                      XINPUT_GAMEPAD_DPAD_UP,
                                      XINPUT_GAMEPAD_DPAD_DOWN,
                                      XINPUT_GAMEPAD_DPAD_LEFT,
                                      XINPUT_GAMEPAD_DPAD_RIGHT,
                                      XINPUT_GAMEPAD_START,
                                      XINPUT_GAMEPAD_BACK};
                for (int b = 0; b < COUNT; ++b)
                    if (pad.Gamepad.wButtons & flags[b])
                        s.padButtons |= 1u << b;
                auto axis = [](SHORT v) { return float(v) / (v < 0 ? 32768.f : 32767.f); };
                s.sticks = {pad.Gamepad.bLeftTrigger / 255.f, pad.Gamepad.bRightTrigger / 255.f,
                            axis(pad.Gamepad.sThumbLX),       axis(pad.Gamepad.sThumbLY),
                            axis(pad.Gamepad.sThumbRX),       axis(pad.Gamepad.sThumbRY)};
                s.padConnected = a.gamepad = true;
                break;
            }
        return a;
    }
};
std::unique_ptr<InputBackend> CreateXInputBackend()
{
    return std::make_unique<XInputBackend>();
}
} // namespace AtomEngine
