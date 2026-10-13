#include "DebugCamera.h"
#include "Runtime/Function/Input/Input.h"
namespace AtomEngine
{
DebugCamera::DebugCamera(Camera &camera)
    : CameraController(camera), mMoveSpeed(5.0f), mRotateSpeed(0.005f), mPanSpeed(0.002f), mZoomSpeed(1.0f)
{
    mCurrentPos = camera.GetPosition();
    mTargetPos = mCurrentPos;
    mCurrentRot = camera.GetRotation();
    mTargetRot = mCurrentRot;

    mRadius = std::max(1.f, mCurrentPos.Length());
    SetTarget(mCurrentPos + camera.GetForwardVec().NormalizedCopy() * mRadius);
}

void DebugCamera::SetTarget(const Vector3 &target)
{
    mTarget = target;
    mCurrentPos = mTargetCamera.GetPosition();
    mTargetPos = mCurrentPos;
    mCurrentRot = mTargetCamera.GetRotation();
    mTargetRot = mCurrentRot;
    auto offset = mCurrentPos - mTarget;
    mRadius = offset.Length();
    if (mRadius > .0001f)
    {
        mYaw = atan2f(offset.x, offset.z);
        mPitch = asinf(std::clamp(offset.y / mRadius, -1.f, 1.f));
    }
}
void DebugCamera::ResetInputTracking()
{
    mInput->GetMousePosition(&mPrevMouseX, &mPrevMouseY);
    mPrevMouseWheel = float(mInput->GetMouseState().wheelY);
    mFirstFrame = false;
}
void DebugCamera::Update(float dt)
{
    int x, y;
    mInput->GetMousePosition(&x, &y);
    if (mFirstFrame)
    {
        ResetInputTracking();
    }
    DebugCameraInput input;
    input.dx = float(x - mPrevMouseX);
    input.dy = float(y - mPrevMouseY);
    auto wheel = float(mInput->GetMouseState().wheelY);
    input.wheel = (wheel - mPrevMouseWheel) / 120.f;
    mPrevMouseX = x;
    mPrevMouseY = y;
    mPrevMouseWheel = wheel;
    input.fly = mInput->IsPressKey(VK_SHIFT);
    input.boost = input.fly;
    input.orbit = mInput->IsPressMouse(0) && mInput->IsPressKey(VK_MENU);
    input.pan = mInput->IsPressMouse(2);
    input.look = input.fly && mInput->IsPressMouse(1);
    input.zoom = !input.fly && mInput->IsPressMouse(1);
    input.movement = Vector3(float(mInput->IsPressKey('D')) - float(mInput->IsPressKey('A')),
                             float(mInput->IsPressKey('E')) - float(mInput->IsPressKey('Q')),
                             float(mInput->IsPressKey('W')) - float(mInput->IsPressKey('S')));
    Update(dt, input);
}
void DebugCamera::Update(float dt, const DebugCameraInput &input)
{
    auto mode = input.fly ? CameraMode::Fly : CameraMode::Orbit;
    if (mode != mMode)
    {
        mCurrentPos = mTargetCamera.GetPosition();
        mTargetPos = mCurrentPos;
        mCurrentRot = mTargetCamera.GetRotation();
        mTargetRot = mCurrentRot;
        auto forward = mTargetCamera.GetForwardVec().NormalizedCopy();
        if (mode == CameraMode::Fly)
        {
            mYaw = atan2f(forward.x, forward.z);
            mPitch = asinf(std::clamp(forward.y, -1.f, 1.f));
        }
        else
        {
            mTarget = mCurrentPos + forward * mRadius;
            auto offset = mCurrentPos - mTarget;
            mYaw = atan2f(offset.x, offset.z);
            mPitch = asinf(std::clamp(offset.y / mRadius, -1.f, 1.f));
        }
        mMode = mode;
    }
    if (mMode == CameraMode::Orbit)
    {
        if (input.orbit)
            UpdateOrbit(input.dx, input.dy);
        else if (input.pan)
            UpdatePan(input.dx, input.dy);
        else if (input.zoom)
            UpdateZoom(input.dy);
        if (input.wheel != 0)
            UpdateZoom(input.wheel * 6.f);
        Vector3 eye = mTarget + Vector3(mRadius * cosf(mPitch) * sinf(mYaw), mRadius * sinf(mPitch),
                                        mRadius * cosf(mPitch) * cosf(mYaw));
        mTargetPos = eye;
        mTargetRot = Quaternion::LookRotation(mTarget - eye, Vector3::UP);
    }
    else
    {
        if (input.look)
            UpdateRotation(input.dx, input.dy);
        UpdateMovement(dt, input);
    }
    mCurrentPos = VInterpTo(mCurrentPos, mTargetPos, dt, 8.f);
    mCurrentRot = QInterpTo(mCurrentRot, mTargetRot, dt, 8.f);
    mTargetCamera.SetPosition(mCurrentPos);
    mTargetCamera.SetRotation(mCurrentRot);
    mTargetCamera.Update();
}

void DebugCamera::UpdateOrbit(float dx, float dy)
{
    mYaw -= dx * mRotateSpeed;
    mPitch -= dy * mRotateSpeed;

    mPitch = std::clamp(mPitch, -Math::HalfPI + 0.1f, Math::HalfPI - 0.1f);
}

void DebugCamera::UpdatePan(float dx, float dy)
{
    Vector3 right = mTargetCamera.GetRightVec();
    Vector3 up = mTargetCamera.GetUpVec();

    mTarget += (-right * dx * mPanSpeed) + (up * dy * mPanSpeed);
}

void DebugCamera::UpdateZoom(float dy)
{
    mRadius *= (1.0f - dy * mZoomSpeed * 0.01f);
    mRadius = Math::Max(mRadius, 0.1f);
}

void DebugCamera::UpdateMovement(float dt, const DebugCameraInput &input)
{
    auto direction = input.movement;
    if (direction.LengthSqr() > 1.f)
        direction.Normalize();
    float speed = mMoveSpeed * (input.boost ? mBoostMultiplier : 1.f);
    mTargetPos += (mTargetRot * direction) * speed * dt;
}

void DebugCamera::UpdateRotation(float dx, float dy)
{
    mYaw += dx * mLookSensitivity;
    mPitch -= dy * mLookSensitivity;
    mPitch = std::clamp(mPitch, -Math::HalfPI + .05f, Math::HalfPI - .05f);
    auto forward = Vector3(sinf(mYaw) * cosf(mPitch), sinf(mPitch), cosf(mYaw) * cosf(mPitch));
    mTargetRot = Quaternion::LookRotation(forward, Vector3::UP);
}
} // namespace AtomEngine
