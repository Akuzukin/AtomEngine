#pragma once
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Function/Physics/PhysicsWorld.h"
#include "Runtime/Function/Scripting/ScriptSystem.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include <set>
namespace AtomEngine
{
class SceneRuntime
{
  public:
    void Update(World& w, float dt, bool playing, bool allowUiInput = true, const Vector2* pointer = nullptr, bool paused = false);
    void Reset();
    std::string ReadScriptFields(World &world,Entity e,size_t index){return mScripts.ReadFields(world,e,index);}
    bool ReloadScripts(const std::filesystem::path &path){return mScripts.ReloadAssembly(path);}
    const std::string &ScriptError() const
    {
        return mScripts.Error();
    }

  private:
    std::unique_ptr<PhysicsWorld> mPhysics;
    ScriptSystem mScripts;
    std::map<Entity,AudioSourceComponent> mAudioStarted;
    std::map<Entity,uint32_t> mParticleEmitters;
    std::weak_ptr<void> mAudioLifetime;
    bool mPlaying = false;
    float mAccumulator = 0;
};
} // namespace AtomEngine
