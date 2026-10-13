#pragma once
#include "CameraController.h"
#include "Runtime/Core/Math/MathInclude.h"

namespace AtomEngine
{
class Input;
struct DebugCameraInput
{
    float dx = 0, dy = 0, wheel = 0;
    bool orbit = false, pan = false, zoom = false, look = false, fly = false, boost = false;
    Vector3 movement = Vector3::ZERO;
};
class DebugCamera : public CameraController
{
  public:
    DebugCamera(Camera &camera);

    void Update(float dt) override;
    void Update(float dt, const DebugCameraInput &input);
    void ResetInputTracking();

    void SetTarget(const Vector3 &target);
    void SetDistance(float dist)
    {
        mRadius = dist;
    }
    float GetDistance() const
    {
        return mRadius;
    }
    void SetMoveSpeed(float speed)
    {
        mMoveSpeed = speed;
    }
    void SetLookSensitivity(float sens)
    {
        mLookSensitivity = sens;
    }

  private:
    void UpdateOrbit(float dx, float dy);
    void UpdatePan(float dx, float dy);
    void UpdateZoom(float dy);
    void UpdateMovement(float dt, const DebugCameraInput &input);
    void UpdateRotation(float dx, float dy);

  private:
    enum class CameraMode
    {
        Orbit,
        Fly
    };
    CameraMode mMode = CameraMode::Orbit;
    Vector3 mTarget;
    float mRadius = 5.0f;

    float mMoveSpeed;
    float mBoostMultiplier = 3.0f;
    float mLookSensitivity = 0.0025f;

    Vector3 mCurrentPos;
    Vector3 mTargetPos;
    Quaternion mCurrentRot;
    Quaternion mTargetRot;

    float mYaw;
    float mPitch;

    float mRotateSpeed;
    float mPanSpeed;
    float mZoomSpeed;

    int mPrevMouseX = 0;
    int mPrevMouseY = 0;
    float mPrevMouseWheel = 0.0f;
    bool mFirstFrame = true;
};
} // namespace AtomEngine
