#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
namespace AtomEngine
{
struct PhysicsContact
{
    Entity first, second;
    bool trigger;
};
class PhysicsWorld
{
  public:
    PhysicsWorld();
    ~PhysicsWorld();
    void Update(World &world, float dt);
    void Reset();
    const std::vector<PhysicsContact> &Contacts() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} // namespace AtomEngine
