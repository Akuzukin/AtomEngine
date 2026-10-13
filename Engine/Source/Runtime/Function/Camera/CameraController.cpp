#include "CameraController.h"
#include "Runtime/Function/Input/Input.h"

namespace AtomEngine
{
    CameraController::CameraController(Camera& camera) : mTargetCamera(camera)
    {
        mInput = Input::GetInstance();
    }
    void CameraController::ApplyMomentum(float& oldValue, float& newValue, float deltaTime)
    {
        float blendedValue;
        if (Math::Abs(newValue) > Math::Abs(oldValue))
            blendedValue = Math::Lerp(newValue, oldValue, Math::Pow(0.6f, deltaTime * 60.0f));
        else
            blendedValue = Math::Lerp(newValue, oldValue, Math::Pow(0.8f, deltaTime * 60.0f));
        oldValue = blendedValue;
        newValue = blendedValue;
    }

    Vector3 CameraController::VInterpTo(const Vector3& current, const Vector3& target, float dt, float speed)
    {
        if (speed <= 0.0f) return target;

        Vector3 delta = target - current;
        float dist = delta.Length();

        if (dist < 0.0001f) return target;

        float move = dist * std::clamp(dt * speed, 0.0f, 1.0f);
        return current + delta.NormalizedCopy() * move;
    }
    Quaternion CameraController::QInterpTo(const Quaternion& current, const Quaternion& target, float dt, float speed)
    {
        if (speed <= 0.0f) return target;
        float alpha = std::clamp(dt * speed, 0.0f, 1.0f);
        auto rotation=Quaternion::Dot(current,target)<0.f ? -target : target;
        return Quaternion::Slerp(current, rotation, alpha).NormalizeCopy();
    }
}

