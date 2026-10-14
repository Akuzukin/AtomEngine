#pragma once
#include "SceneSerializer.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include <cmath>
namespace AtomEngine {
class SceneAuthoring {
public:
    static std::vector<Entity> CreatePool(World &world,Entity source,int count,int columns,const Vector3 &spacing,bool inactive,const std::filesystem::path &prefab={}) {
        if(count<1 || count>256 || columns<1 || columns>256 || !std::isfinite(spacing.x)||!std::isfinite(spacing.y)||!std::isfinite(spacing.z))throw std::runtime_error("Pool count/columns must be 1..256; spacing must be finite");
        if(prefab.empty() && !world.IsValid(source))throw std::runtime_error("Select an object or choose a prefab");
        auto id=world.IsValid(source)?world.GetComponent<PersistentIdComponent>(source).value:std::string{};
        World staged;SceneSerializer::FromXml(staged,SceneSerializer::ToXml(world,"Pool"));
        auto original=staged.FindById(id);std::vector<Entity> created;
        for(int i=0;i<count;++i){auto e=prefab.empty()?SceneSerializer::Duplicate(staged,original):SceneSerializer::InstantiatePrefab(staged,prefab);
            auto &transform=staged.GetComponent<TransformComponent>(e);transform.transition+=Vector3((i%columns)*spacing.x,(i/columns)*spacing.y,(i/columns)*spacing.z);
            staged.GetComponent<ObjectStateComponent>(e).activeSelf=!inactive;
            auto name=staged.TryGet<NameComponent>(e);staged.SetName(e,(name?name->value:"Pool object")+" "+std::to_string(i+1));created.push_back(e);
            if(staged.View<PersistentIdComponent>().size()>4096)throw std::runtime_error("Batch pool exceeds 4096 objects");
        }
        SceneSerializer::ResolveHierarchy(staged);world.ReplaceWith(staged);return created;
    }
};
}
