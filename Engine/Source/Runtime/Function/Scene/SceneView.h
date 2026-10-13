#pragma once
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "Runtime/Function/Camera/CameraBase.h"
namespace AtomEngine
{
inline Entity SceneCamera(World &world)
{
    Entity selected = entt::null;
    for (auto e : world.View<CameraComponent, TransformComponent>())
    {
        const auto &c = world.GetComponent<CameraComponent>(e);
        if (c.enabled && c.primary &&
            (selected == entt::null || world.GetComponent<PersistentIdComponent>(e).value <
                                           world.GetComponent<PersistentIdComponent>(selected).value))
            selected = e;
    }
    return selected;
}
inline void ApplySceneCamera(World &world, Camera &camera, Entity entity = entt::null)
{
    if (entity == entt::null)
        entity = SceneCamera(world);
    if (entity == entt::null)
        return;
    const auto &settings = world.GetComponent<CameraComponent>(entity);
    const auto matrix = world.GetComponent<TransformComponent>(entity).GetMatrix();
    camera.SetPosition(matrix.GetTrans());
    camera.SetRotation(matrix.GetRotation());
    camera.SetFOV(settings.fieldOfView * Math::PI / 180.f);
    camera.SetZRange(settings.nearClip, settings.farClip);
    camera.Update();
}
inline void EnsureSceneViewObjects(World &world, const Camera &camera)
{
    if (world.View<CameraComponent>().begin() == world.View<CameraComponent>().end())
    {
        auto object = world.CreateGameObject("Main Camera");
        object->AddComponent<TransformComponent>(camera.GetPosition(), camera.GetRotation(), Vector3::UNIT_SCALE);
        object->AddComponent<CameraComponent>();
        auto &c = object->GetComponent<CameraComponent>();
        c.fieldOfView = camera.GetFOV() * 180.f / Math::PI;
        c.nearClip = camera.GetNearClip();
        c.farClip = camera.GetFarClip();
    }
    if (world.View<DirectionalLightComponent>().begin() == world.View<DirectionalLightComponent>().end())
    {
        Camera basis;
        basis.SetLookDirection(Vector3(.35f, -1, .25f).NormalizedCopy());
        auto object = world.CreateGameObject("Sun");
        object->AddComponent<TransformComponent>(Vector3::ZERO, basis.GetRotation(), Vector3::UNIT_SCALE);
        object->AddComponent<DirectionalLightComponent>();
    }
}
} // namespace AtomEngine
