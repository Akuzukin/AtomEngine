#include "SceneRuntime.h"
#include "Runtime/Function/Render/Particle/ParticleEmitterRuntime.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "SceneSerializer.h"
#include <algorithm>
#include "json.hpp"
#include "Runtime/Function/Audio/Audio.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
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
        if(mAudioLifetime.lock()!=w.GetLifetime().lock()){Reset();mAudioLifetime=w.GetLifetime();}
        for(auto e:w.View<ParticleComponent>()){auto &emitter=w.GetComponent<ParticleComponent>(e);if(emitter.runtimeId!=0xffffffff)mParticleEmitters[e]=emitter.runtimeId;}
        for(auto it=mParticleEmitters.begin();it!=mParticleEmitters.end();)if(!w.IsValid(it->first)||!w.HasComponent<ParticleComponent>(it->first)){bool reused=false;for(auto owner:w.View<ParticleComponent>())if(w.GetComponent<ParticleComponent>(owner).runtimeId==it->second){reused=true;break;}if(!reused)ParticleSystem::ReleaseParticle(it->second);it=mParticleEmitters.erase(it);}else ++it;
        return;
    }
    if (mAudioLifetime.lock() != w.GetLifetime().lock())
    {
        Reset(); mAudioLifetime = w.GetLifetime();
    }
    auto audio = Audio::GetInstance();
    audio->Update(); audio->SetPaused(paused);
    if(paused){mScripts.PauseTime();mScripts.SynchronizeParameters(w);return;}
    for (auto e : w.View<AudioSourceComponent>()) {
        auto &source = w.GetComponent<AudioSourceComponent>(e);
        auto previous=mAudioStarted.find(e);
        if(previous!=mAudioStarted.end()) {
            const auto &old=previous->second;
            if(old.clip!=source.clip||old.volume!=source.volume||old.loop!=source.loop||old.playOnAwake!=source.playOnAwake||old.bus!=source.bus) {
                audio->StopOwner(uint64_t(uint32_t(e))+1);mAudioStarted.erase(previous);
            }
        }
        if (w.IsEnabled(e,ComponentKind::Audio) && !mAudioStarted.contains(e))
        {
            mAudioStarted.emplace(e,source);
            if (!source.clip.empty()) try {
                Audio::Load(source.clip);
                if (source.playOnAwake) audio->PlayClip(source.clip, source.volume, source.loop, uint64_t(uint32_t(e)) + 1,source.bus);
            } catch (const std::exception &error) { WriteLog(LogLevel::Warning, "Audio", source.clip + ": " + error.what()); }
        }
    }
    for (auto it = mAudioStarted.begin(); it != mAudioStarted.end();)
        if (!w.IsValid(it->first) || !w.HasComponent<AudioSourceComponent>(it->first)) {
            audio->StopOwner(uint64_t(uint32_t(it->first)) + 1); it = mAudioStarted.erase(it);
        } else ++it;
    for(auto e:w.View<PersistentIdComponent>())audio->SetOwnerPaused(uint64_t(uint32_t(e))+1,!w.IsEnabled(e,ComponentKind::Audio));
    mPlaying = true;
    if (!mPhysics)
        mPhysics = std::make_unique<PhysicsWorld>();
    mScripts.SetPhysics(mPhysics.get());
    auto &pointerMouse=Input::GetInstance()->GetMouseState();
    w.gameplayPointer=pointer?*pointer:Vector2(float(pointerMouse.positionX),float(pointerMouse.positionY));
    w.gameplayPointerValid=allowUiInput && Input::GetInstance()->GameplayInputEnabled() &&
        w.gameplayPointer.x>=0 && w.gameplayPointer.y>=0 && w.gameplayPointer.x<w.uiRenderSize.x && w.gameplayPointer.y<w.uiRenderSize.y;
    mScripts.Update(w, dt);
    mScripts.FlushCommands(w);
    for(auto e:w.View<ParticleComponent>()) {
        auto &p=w.GetComponent<ParticleComponent>(e);
        if(ParticleSystem::GetCurrentLife(p.runtimeId)<0)p.runtimeId=0xffffffff;
        if(p.playOnAwake&&p.runtimeId==0xffffffff&&w.IsEnabled(e,ComponentKind::Particle)) {
            try {auto t=w.TryGet<TransformComponent>(e);EmitSceneParticle(w,e,t?t->GetWorldPosition():Vector3::ZERO);}catch(const std::exception &error){WriteLog(LogLevel::Warning,"Particles",error.what());}
        }
        if(p.runtimeId!=0xffffffff){mParticleEmitters[e]=p.runtimeId;if(!w.IsEnabled(e,ComponentKind::Particle))ParticleSystem::StopParticle(p.runtimeId);}
    }
    for(auto it=mParticleEmitters.begin();it!=mParticleEmitters.end();)if(!w.IsValid(it->first)||!w.HasComponent<ParticleComponent>(it->first)){bool reused=false;for(auto owner:w.View<ParticleComponent>())if(w.GetComponent<ParticleComponent>(owner).runtimeId==it->second){reused=true;break;}if(!reused)ParticleSystem::ReleaseParticle(it->second);it=mParticleEmitters.erase(it);}else ++it;

    for(auto e:w.View<PersistentIdComponent>())audio->SetOwnerPaused(uint64_t(uint32_t(e))+1,!w.IsEnabled(e,ComponentKind::Audio));
    if (allowUiInput && Input::GetInstance()->IsTriggerMouse(0))
    {
        auto &mouse = Input::GetInstance()->GetMouseState();
        auto entity = UiSystem::HitTest(w, pointer ? pointer->x : float(mouse.positionX), pointer ? pointer->y : float(mouse.positionY));
        if (entity != entt::null){w.uiFocus=entity;mScripts.UiClick(w, entity);}
    }
    if(Input::GetInstance()->GameplayInputEnabled()) {
        auto input=Input::GetInstance();int direction=0;
        if(input->IsTriggerKey(KeyCode::Tab))direction=input->IsPressKey(KeyCode::Shift)?-1:1;
        if(input->IsTriggerKey(KeyCode::Left)||input->IsTriggerKey(KeyCode::Up)||input->IsTriggerGamePad(DPadLeft)||input->IsTriggerGamePad(DPadUp))direction=-1;
        if(input->IsTriggerKey(KeyCode::Right)||input->IsTriggerKey(KeyCode::Down)||input->IsTriggerGamePad(DPadRight)||input->IsTriggerGamePad(DPadDown))direction=1;
        auto focused=UiSystem::Navigate(w,direction);
        if(focused!=entt::null && (input->IsTriggerKey(KeyCode::Enter)||input->IsTriggerKey(KeyCode::Space)||input->IsTriggerGamePad(A)))mScripts.UiClick(w,focused);
    }
    mAccumulator += std::clamp(dt, 0.f, .25f);
    constexpr float step = 1.f / 60;
    while (mAccumulator >= step)
    {
        mScripts.FixedUpdate(w, step);
        mScripts.FlushCommands(w);
        SceneSerializer::ResolveHierarchy(w);
        mPhysics->Update(w, step);
        auto contacts=mPhysics->Contacts();
        for (auto &c : contacts)
            mScripts.Collision(w, c);
        mScripts.FlushCommands(w);
        mAccumulator -= step;
    }
}
void SceneRuntime::Reset()
{
    mScripts.Reset();
    for(auto [e,id]:mParticleEmitters)ParticleSystem::ReleaseParticle(id);
    mParticleEmitters.clear();
    Audio::GetInstance()->StopAll();
    mAudioStarted.clear(); mAudioLifetime.reset();
    mPhysics.reset();
    mPlaying = false;
    mAccumulator = 0;
}
} // namespace AtomEngine
