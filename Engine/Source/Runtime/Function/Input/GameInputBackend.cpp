#include "InputBackend.h"
#include <GameInput.h>
#include <wrl.h>
#include <vector>
#if GAMEINPUT_API_VERSION == 1
using namespace GameInput::v1;
#elif GAMEINPUT_API_VERSION == 2
using namespace GameInput::v2;
#elif GAMEINPUT_API_VERSION == 3
using namespace GameInput::v3;
#endif
namespace AtomEngine
{
class GameInputBackend final : public InputBackend
{
    Microsoft::WRL::ComPtr<IGameInput> mInput;
    static bool Connected(IGameInputReading *r)
    {
        Microsoft::WRL::ComPtr<IGameInputDevice> device;
        r->GetDevice(&device);
        return device && (device->GetDeviceStatus() & GameInputDeviceConnected) != 0;
    }

  public:
    bool Initialize() override
    {
        return SUCCEEDED(GameInputCreate(&mInput));
    }
    const char *Name() const override
    {
        return "GameInput";
    }
    InputAvailability Poll(InputSnapshot &s) override
    {
        InputAvailability a{};
        if (!mInput)
            return a;
        Microsoft::WRL::ComPtr<IGameInputReading> r;
        if (SUCCEEDED(mInput->GetCurrentReading(GameInputKindKeyboard, nullptr, &r)) && Connected(r.Get()))
        {
            std::vector<GameInputKeyState> keys(r->GetKeyCount());
            auto count = keys.empty() ? 0 : r->GetKeyState(uint32_t(keys.size()), keys.data());
            for (uint32_t i = 0; i < count; ++i)
                if (keys[i].virtualKey < 256)
                    s.keys[keys[i].virtualKey] = true;
            a.keyboard = true;
        }
        r.Reset();
        GameInputMouseState mouse{};
        if (SUCCEEDED(mInput->GetCurrentReading(GameInputKindMouse, nullptr, &r)) && Connected(r.Get()) &&
            r->GetMouseState(&mouse))
        {
            s.mouse.buttons = uint32_t(mouse.buttons) & 31;
            a.mouse = true;
        }
        r.Reset();
        GameInputGamepadState pad{};
        if (SUCCEEDED(mInput->GetCurrentReading(GameInputKindGamepad, nullptr, &r)) && Connected(r.Get()) &&
            r->GetGamepadState(&pad))
        {
            const GameInputGamepadButtons flags[] = {GameInputGamepadA,
                                                     GameInputGamepadB,
                                                     GameInputGamepadX,
                                                     GameInputGamepadY,
                                                     GameInputGamepadLeftShoulder,
                                                     GameInputGamepadRightShoulder,
                                                     GameInputGamepadLeftThumbstick,
                                                     GameInputGamepadRightThumbstick,
                                                     GameInputGamepadDPadUp,
                                                     GameInputGamepadDPadDown,
                                                     GameInputGamepadDPadLeft,
                                                     GameInputGamepadDPadRight,
                                                     GameInputGamepadMenu,
                                                     GameInputGamepadView};
            for (int i = 0; i < COUNT; ++i)
                if (pad.buttons & flags[i])
                    s.padButtons |= 1u << i;
            s.sticks = {pad.leftTrigger,     pad.rightTrigger,     pad.leftThumbstickX,
                        pad.leftThumbstickY, pad.rightThumbstickX, pad.rightThumbstickY};
            s.padConnected = a.gamepad = true;
        }
        return a;
    }
};
std::unique_ptr<InputBackend> CreateGameInputBackend()
{
    return std::make_unique<GameInputBackend>();
}
} // namespace AtomEngine
