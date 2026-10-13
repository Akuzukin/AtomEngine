#include"GameObject.h"
#include "World.h"

namespace AtomEngine
{
    GameObject::GameObject(Entity handle, World* world)
        : mHandle(handle), mWorldLifetime(world ? world->GetLifetime() : std::weak_ptr<void>{}), mWorld(world)
    {
    }

    bool GameObject::IsValid() const
    {
        return !mWorldLifetime.expired() && mWorld != nullptr && mWorld->IsValid(mHandle);
    }

    void GameObject::Destroy()
    {
        if (IsValid()) mWorld->DestroyGameObject(mHandle);
        mHandle = entt::null;
    }

    std::string GameObject::GetName()
    {
        if (HasComponent<NameComponent>())
            return GetComponent<NameComponent>().value;
        return {};
    }

    void GameObject::SetName(const std::string& name)
    {
       mWorld->SetName(mHandle, name);
    }

}
