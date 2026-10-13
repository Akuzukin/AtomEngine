#include "World.h"
#include "GameObject.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include <atomic>
#include <chrono>

#include "Runtime/Core/LogSystem/LogSystem.h"

namespace AtomEngine
{
World::World()
{
    mDispatcher.sink<ComponentAddedEvent>().connect<&World::OnComponentAdded>(this);
    mDispatcher.sink<ComponentRemovedEvent>().connect<&World::OnComponentRemoved>(this);
}

World::~World()
{
    mLifetime.reset();
    mRegistry.clear();
    mDispatcher.clear();
}

std::unique_ptr<GameObject> World::CreateGameObject(const std::string &name)
{
    Entity e = mRegistry.create();
    auto obj = std::make_unique<GameObject>(e, this);
    static std::atomic<uint64_t> ids{uint64_t(std::chrono::system_clock::now().time_since_epoch().count())};
    obj->AddComponent<PersistentIdComponent>(std::to_string(++ids));
    if (!name.empty())
    {

        obj->AddComponent<NameComponent>(name);
    }
    obj->AddComponent<DeathFlag>();
    LogDebug("[Object Added]:%s -> Entity(%d)\n", name.c_str(), (uint32_t)e);

    return std::move(obj);
}

std::unique_ptr<GameObject> World::GetGameObject(Entity entity)
{
    return IsValid(entity) ? std::make_unique<GameObject>(entity, this) : nullptr;
}

void World::ReplaceWith(World &source)
{
    mLifetime.reset();
    mRegistry.swap(source.mRegistry);
    std::swap(settings, source.settings);
    mLifetime = std::make_shared<int>(0);
}

void World::DestroyGameObject(Entity entity)
{
    if (!IsValid(entity))
        return;

    mRegistry.destroy(entity);
}

Entity World::FindByName(const std::string &name)
{
    auto view = mRegistry.view<NameComponent>();
    for (auto entity : view)
        if (view.get<NameComponent>(entity).value == name)
            return entity;
    return entt::null;
}

void World::SetName(Entity entity, const std::string &name)
{
    if (IsValid(entity))
        mRegistry.emplace_or_replace<NameComponent>(entity, name);
}

void World::OnComponentAdded(const ComponentAddedEvent &e)
{
    if (mOnComponentAdded)
        mOnComponentAdded(e.entity, e.type);
    std::string typeName = e.type.name();
    LogDebug("[Component Added]:%s -> Entity(%d)\n", typeName.c_str(), (uint32_t)e.entity);
}

void World::OnComponentRemoved(const ComponentRemovedEvent &e)
{
    if (mOnComponentRemoved)
        mOnComponentRemoved(e.entity, e.type);
    std::string typeName = e.type.name();
    LogDebug("[Component Removed]:%s -> Entity(%d)\n", typeName.c_str(), (uint32_t)e.entity);
}
} // namespace AtomEngine
