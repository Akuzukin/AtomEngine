#include "SceneRuntime.h"
#include "SceneSerializer.h"
#include <algorithm>
#include "Runtime/Function/UI/UiSystem.h"
#include "Runtime/Function/Input/Input.h"
namespace AtomEngine
{
void SceneRuntime::Update(World &w, float dt, bool playing, bool allowUiInput, const Vector2* pointer, bool paused)
{
    SceneSerializer::ResolveHierarchy(w);
    if (!playing)
    {
        if (mPlaying)
            Reset();
        return;
    }
    if(paused) return;
    mPlaying = true;
    if (!mPhysics)
        mPhysics = std::make_unique<PhysicsWorld>();
    mScripts.Update(w, dt);
    if (allowUiInput && Input::GetInstance()->IsTriggerMouse(0))
    {
        auto &mouse = Input::GetInstance()->GetMouseState();
        auto entity = UiSystem::HitTest(w, pointer ? pointer->x : float(mouse.positionX), pointer ? pointer->y : float(mouse.positionY));
        if (entity != entt::null)
            mScripts.UiClick(w, entity);
    }
    mAccumulator += std::clamp(dt, 0.f, .25f);
    constexpr float step = 1.f / 60;
    while (mAccumulator >= step)
    {
        mScripts.FixedUpdate(w, step);
        SceneSerializer::ResolveHierarchy(w);
        mPhysics->Update(w, step);
        for (auto &c : mPhysics->Contacts())
            mScripts.Collision(w, c);
        mAccumulator -= step;
    }
}
void SceneRuntime::Reset()
{
    mScripts.Reset();
    mPhysics.reset();
    mPlaying = false;
    mAccumulator = 0;
}
} // namespace AtomEngine
