#include "Input.h"
#include "Runtime/Function/Render/WindowManager.h"
#include <cmath>
namespace AtomEngine
{
Input *Input::GetInstance()
{
    static Input input;
    return &input;
}
void Input::Initialize()
{
    Shutdown();
    mPrimary = CreateGameInputBackend();
    if (!mPrimary->Initialize())
        mPrimary.reset();
    mFallback = CreateXInputBackend();
    mFallback->Initialize();
}
void Input::Shutdown()
{
    mGameplayInputEnabled = true;
    mPrimary.reset();
    mFallback.reset();
    mState = {};
    mPrevious = {};
    mWheel = 0;
    mRepeatPressed = false;
    mDevice = InputDeviceType::None;
    mPadBackend = "Disconnected";
}
void Input::Update()
{
    mPrevious = mState;
    mState = {};
    InputSnapshot fallback{};
    if (mFallback)
        mFallback->Poll(fallback);
    mState = fallback;
    InputSnapshot primary{};
    InputAvailability available{};
    if (mPrimary)
        available = mPrimary->Poll(primary);
    if (available.keyboard)
        mState.keys = primary.keys;
    if (available.mouse)
        mState.mouse.buttons = primary.mouse.buttons;
    mState.mouse.positionX = fallback.mouse.positionX;
    mState.mouse.positionY = fallback.mouse.positionY;
    mState.mouse.wheelY = mWheel;
    if (available.gamepad)
    {
        mState.sticks = primary.sticks;
        mState.padButtons = primary.padButtons;
        mState.padConnected = true;
    }
    mPadBackend = mState.padConnected ? (available.gamepad ? "GameInput" : "XInput") : "Disconnected";
    mState.NormalizeModifiers();
    if (!WindowManager::GetInstance()->HasInputFocus())
    {
        mState.keys = {};
        mState.mouse.buttons = 0;
        mState.padButtons = 0;
        mState.sticks = {};
    }
    for (bool down : mState.keys)
        if (down)
            mDevice = InputDeviceType::Keyboard;
    if (mState.mouse.buttons || mState.mouse.positionX != mPrevious.mouse.positionX ||
        mState.mouse.positionY != mPrevious.mouse.positionY)
        mDevice = InputDeviceType::Mouse;
    if (mState.padButtons || std::abs(mState.sticks.leftStickX) > 0.15f || std::abs(mState.sticks.leftStickY) > 0.15f ||
        std::abs(mState.sticks.rightStickX) > 0.15f || std::abs(mState.sticks.rightStickY) > 0.15f)
        mDevice = InputDeviceType::Gamepad;
}
static uint32_t MouseBit(int b)
{
    return b >= 0 && b < 5 ? 1u << b : 0;
}
static uint32_t PadBit(GamePadButton b)
{
    return b >= 0 && b < COUNT ? 1u << b : 0;
}
bool Input::IsPressMouse(int b) const
{
    return (mState.mouse.buttons & MouseBit(b)) != 0;
}
bool Input::IsTriggerMouse(int b) const
{
    return IsPressMouse(b) && !(mPrevious.mouse.buttons & MouseBit(b));
}
bool Input::IsReleaseMouse(int b) const
{
    return !IsPressMouse(b) && (mPrevious.mouse.buttons & MouseBit(b));
}
bool Input::IsPressGamePad(GamePadButton b) const
{
    return (mState.padButtons & PadBit(b)) != 0;
}
bool Input::IsTriggerGamePad(GamePadButton b) const
{
    return IsPressGamePad(b) && !(mPrevious.padButtons & PadBit(b));
}
bool Input::IsReleaseGamePad(GamePadButton b) const
{
    return !IsPressGamePad(b) && (mPrevious.padButtons & PadBit(b));
}
bool Input::GetMousePosition(int *x, int *y)
{
    if (!x || !y)
        return false;
    *x = int(mState.mouse.positionX);
    *y = int(mState.mouse.positionY);
    return true;
}
bool Input::IsStickTriggerRepeat(float v, float t, float dt, float delay, float interval) const
{
    if (std::abs(v) <= t)
    {
        mRepeatPressed = false;
        mRepeatTimer = 0;
        return false;
    }
    if (!mRepeatPressed)
    {
        mRepeatPressed = true;
        mRepeatTimer = delay;
        return true;
    }
    mRepeatTimer -= dt;
    if (mRepeatTimer <= 0)
    {
        mRepeatTimer = interval;
        return true;
    }
    return false;
}
} // namespace AtomEngine
