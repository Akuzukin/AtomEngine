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
    obj->AddComponent<ObjectStateComponent>();
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
    mPersistentIndex.clear();
    mRegistry.swap(source.mRegistry);
    uiFocus=entt::null;
    std::swap(settings, source.settings);
    mLifetime = std::make_shared<int>(0);
}

void World::DestroyGameObject(Entity entity)
{
    if (!IsValid(entity))
        return;

    if(auto id=TryGet<PersistentIdComponent>(entity))mPersistentIndex.erase(id->value);
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

Entity World::FindById(const std::string &id) {
    auto cached=mPersistentIndex.find(id);if(cached!=mPersistentIndex.end() && IsValid(cached->second) && TryGet<PersistentIdComponent>(cached->second) && GetComponent<PersistentIdComponent>(cached->second).value==id)return cached->second;
    for(auto e:View<PersistentIdComponent>())if(GetComponent<PersistentIdComponent>(e).value==id){mPersistentIndex[id]=e;return e;}return entt::null;
}
bool World::IsActive(Entity e) {
    // A malformed cycle must never hang runtime systems.
    size_t remaining=mRegistry.view<PersistentIdComponent>().size()+1;
    while(IsValid(e) && remaining--) {
        auto state=TryGet<ObjectStateComponent>(e);if(state && !state->activeSelf)return false;
        auto hierarchy=TryGet<HierarchyComponent>(e);if(!hierarchy || hierarchy->parentId.empty())return true;
        e=FindById(hierarchy->parentId);
    }
    return false;
}
bool World::IsEnabled(Entity e,ComponentKind kind) {
    auto state=IsValid(e)?TryGet<ObjectStateComponent>(e):nullptr;
    return IsActive(e) && (!state || !(state->disabledComponents & (1u<<int(kind))));
}
void World::SetComponentEnabled(Entity e,ComponentKind kind,bool enabled) {
    if(!IsValid(e) || int(kind)<0 || int(kind)>9)throw std::runtime_error("Invalid component state");
    if(!HasComponent<ObjectStateComponent>(e))AddComponent<ObjectStateComponent>(e);
    auto &mask=GetComponent<ObjectStateComponent>(e).disabledComponents;if(enabled)mask &= ~(1u<<int(kind));else mask |= 1u<<int(kind);
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
