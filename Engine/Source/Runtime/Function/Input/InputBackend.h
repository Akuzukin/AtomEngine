#pragma once
#include <array>
#include <cstdint>
#include <memory>
namespace AtomEngine
{
enum GamePadButton
{
    A,
    B,
    X,
    Y,
    LB,
    RB,
    LeftThumbstick,
    RightThumbstick,
    DPadUp,
    DPadDown,
    DPadLeft,
    DPadRight,
    Menu,
    View,
    COUNT
};
struct PadStick
{
    float leftTrigger = 0, rightTrigger = 0, leftStickX = 0, leftStickY = 0, rightStickX = 0, rightStickY = 0;
};
struct MouseState
{
    uint32_t buttons = 0;
    int64_t positionX = 0, positionY = 0, wheelY = 0;
};
enum class InputDeviceType
{
    None,
    Keyboard,
    Mouse,
    Gamepad
};
struct InputSnapshot
{
    std::array<bool, 256> keys{};
    MouseState mouse{};
    PadStick sticks{};
    uint32_t padButtons = 0;
    bool padConnected = false;
    void NormalizeModifiers()
    {
        // Win32 virtual-key values: generic Shift/Ctrl/Alt include either side.
        keys[0x10] = keys[0x10] || keys[0xA0] || keys[0xA1];
        keys[0x11] = keys[0x11] || keys[0xA2] || keys[0xA3];
        keys[0x12] = keys[0x12] || keys[0xA4] || keys[0xA5];
    }
};
struct InputAvailability
{
    bool keyboard = false, mouse = false, gamepad = false;
};
class InputBackend
{
  public:
    virtual ~InputBackend() = default;
    virtual bool Initialize() = 0;
    virtual InputAvailability Poll(InputSnapshot &state) = 0;
    virtual const char *Name() const = 0;
};
std::unique_ptr<InputBackend> CreateGameInputBackend();
std::unique_ptr<InputBackend> CreateXInputBackend();
} // namespace AtomEngine
