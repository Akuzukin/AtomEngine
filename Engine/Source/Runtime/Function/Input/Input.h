#pragma once
#include "InputBackend.h"
namespace AtomEngine
{
class Input
{
  public:
    static Input *GetInstance();
    void Initialize();
    void Shutdown();
    void Update();
    bool IsPressKey(uint8_t k) const
    {
        return mState.keys[k];
    }
    bool IsTriggerKey(uint8_t k) const
    {
        return mState.keys[k] && !mPrevious.keys[k];
    }
    bool IsReleaseKey(uint8_t k) const
    {
        return !mState.keys[k] && mPrevious.keys[k];
    }
    bool IsPressMouse(int b) const;
    bool IsTriggerMouse(int b) const;
    bool IsReleaseMouse(int b) const;
    bool IsPressGamePad(GamePadButton b) const;
    bool IsTriggerGamePad(GamePadButton b) const;
    bool IsReleaseGamePad(GamePadButton b) const;
    bool GetMousePosition(int *x, int *y);
    const MouseState &GetMouseState() const
    {
        return mState.mouse;
    }
    const PadStick &GetGamePadStick() const
    {
        return mState.sticks;
    }
    uint32_t GetGamePadButton() const
    {
        return mState.padButtons;
    }
    bool IsUseGamePad() const
    {
        return mDevice == InputDeviceType::Gamepad;
    }
    bool IsContactGamePad() const
    {
        return mState.padConnected;
    }
    bool IsStickTriggerRepeat(float value, float threshold, float dt, float delay, float interval) const;
    const char *GetGamepadBackendName() const
    {
        return mPadBackend;
    }
    void OnMouseWheel(int delta)
    {
        mWheel += delta;
    }

  private:
    Input() = default;
    std::unique_ptr<InputBackend> mPrimary, mFallback;
    InputSnapshot mState{}, mPrevious{};
    InputDeviceType mDevice = InputDeviceType::None;
    const char *mPadBackend = "Disconnected";
    int64_t mWheel = 0;
    mutable bool mRepeatPressed = false;
    mutable float mRepeatTimer = 0;
};
} // namespace AtomEngine
