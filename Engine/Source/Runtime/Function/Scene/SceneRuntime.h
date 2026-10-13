#pragma once
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Function/Physics/PhysicsWorld.h"
#include "Runtime/Function/Scripting/ScriptSystem.h"
namespace AtomEngine
{
class SceneRuntime
{
  public:
    void Update(World& w, float dt, bool playing, bool allowUiInput = true, const Vector2* pointer = nullptr, bool paused = false);
    void Reset();
    const std::string &ScriptError() const
    {
        return mScripts.Error();
    }

  private:
    std::unique_ptr<PhysicsWorld> mPhysics;
    ScriptSystem mScripts;
    bool mPlaying = false;
    float mAccumulator = 0;
};
} // namespace AtomEngine
