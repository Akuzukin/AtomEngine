#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Physics/PhysicsWorld.h"
namespace AtomEngine
{
class ScriptSystem
{
  public:
    ScriptSystem();
    ~ScriptSystem();
    bool Initialize();
    void Update(World &world, float dt);
    void FixedUpdate(World &world, float dt);
    void UiClick(World &world, Entity entity);
    void Collision(World &world, const PhysicsContact &contact);
    void Reset();
    const std::string &Error() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} // namespace AtomEngine
