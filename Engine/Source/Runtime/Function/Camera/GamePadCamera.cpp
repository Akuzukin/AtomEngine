#include "GamePadCamera.h"
#include "Runtime/Function/Input/Input.h"

namespace AtomEngine
{
    GamePadCamera::GamePadCamera(Camera& camera)
        : CameraController(camera)
    {
        mTarget = Vector3(0, 0, 0);
        mRadius = 12.0f;

        mCurrentPos = camera.GetPosition();
        mCurrentRot = camera.GetRotation();

        Vector3 toEye = mCurrentPos - mTarget;
        mRadius = toEye.Length();

        if (mRadius > 0.0001f)
        {
            mYaw = atan2f(toEye.x, toEye.z);
            mPitch = asinf(toEye.y / mRadius);
        }
    }
    void GamePadCamera::Update(float dt)
    {
        const auto& pad = mInput->GetGamePadStick();

        UpdateOrbit(pad, dt);
        UpdateZoom(pad,dt);

        Vector3 eye;
        eye.x = mTarget.x + mRadius * cosf(mPitch) * sinf(mYaw);
        eye.y = mTarget.y + mRadius * sinf(mPitch);
        eye.z = mTarget.z + mRadius * cosf(mPitch) * cosf(mYaw);

        mTargetPos = eye;
        Quaternion newRot = Quaternion::LookRotation(mTarget - eye, Vector3::UP);
        if (Quaternion::Dot(newRot, mTargetRot) < 0.0f)
            newRot = -newRot;
        mTargetRot = newRot;

        mCurrentPos = VInterpTo(mCurrentPos, mTargetPos, dt, 8.0f);
        mCurrentRot = QInterpTo(mCurrentRot, mTargetRot, dt, 8.0f);

        mTargetCamera.SetPosition(mCurrentPos);
        mTargetCamera.SetRotation(mCurrentRot);
        mTargetCamera.Update();
    }

    void GamePadCamera::UpdateOrbit(const PadStick& pad, float dt)
    {
        float dx = pad.rightStickX;
        float dy = pad.rightStickY;

        if (fabs(dx) < 0.1f && fabs(dy) < 0.1f)
            return;

        mYaw += dx * mLookSensitivity * dt;
        mPitch += -dy * mLookSensitivity * dt;

        constexpr float limit = XMConvertToRadians(89.0f);
        mPitch = std::clamp(mPitch, -limit, limit);
    }
    void GamePadCamera::UpdateZoom(const PadStick& pad,float dt)
    {
        float out = 0, in = 0;
        out += pad.leftTrigger;
        in += pad.rightTrigger;

        mRadius += out + mZoomSpeed * dt;
        mRadius -= in + mZoomSpeed * dt;

        mRadius = std::clamp(mRadius, 1.0f, 120.0f);
    }
}
