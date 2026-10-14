#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
namespace AtomEngine
{
struct PhysicsContact
{
    Entity first, second;
    bool trigger;
};
struct PhysicsHit { Entity entity=entt::null; Vector3 point=Vector3::ZERO, normal=Vector3::ZERO; float distance=0; };
class PhysicsWorld
{
  public:
    PhysicsWorld();
    ~PhysicsWorld();
    void Update(World &world, float dt);
    void Reset();
    bool Raycast(World &world, const Vector3 &origin, const Vector3 &direction, float distance, PhysicsHit &hit, uint32_t mask=0xffffffffu, bool triggers=false, Entity ignore=entt::null);
    std::vector<Entity> OverlapSphere(World &world,const Vector3 &center,float radius,uint32_t mask=0xffffffffu,bool triggers=false,Entity ignore=entt::null);
    const std::vector<PhysicsContact> &Contacts() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} // namespace AtomEngine
