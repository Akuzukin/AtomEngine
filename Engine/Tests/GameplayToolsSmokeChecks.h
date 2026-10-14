#pragma once
#include "Runtime/Function/Scripting/ScriptSystem.h"
#include "Runtime/Function/Audio/Audio.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "json.hpp"
#include "Editor/EditorDocument.h"
#include "Runtime/Function/Scene/SceneRuntime.h"
inline void RunGameplayToolsSmokeChecks()
{
    using namespace AtomEngine;
    World world;
    auto object = world.CreateGameObject("Gameplay tools probe");
    object->AddComponent<TransformComponent>(); object->AddComponent<UiElementComponent>();
    object->AddComponent<AudioSourceComponent>(AudioSourceComponent{"Asset/Sounds/se.mp3",.4f,false,false});
    object->AddComponent<ScriptComponent>();
    object->GetComponent<ScriptComponent>().scripts.push_back({"Scripts/AtomScripts.dll","Game.GameplayToolsProbe",true,{{"RetainForRestart","true"}}});
    auto xml = SceneSerializer::ToXml(world,"Audio fixture"); World roundtrip;
    SceneSerializer::FromXml(roundtrip,xml);
    auto source = roundtrip.GetComponent<AudioSourceComponent>(roundtrip.FindByName("Gameplay tools probe"));
    FeatureCheck(source.clip == "Asset/Sounds/se.mp3" && std::abs(source.volume-.4f)<1e-6f && !source.loop && !source.playOnAwake,"Audio source XML roundtrip failed");
    EditorDocument history; history.Reset(roundtrip,"Audio fixture");
    auto emitter = roundtrip.FindByName("Gameplay tools probe");
    roundtrip.GetComponent<AudioSourceComponent>(emitter).volume=.8f;
    history.Capture(roundtrip,"Audio fixture", "", "Audio volume"); history.Undo(roundtrip);
    emitter=roundtrip.FindByName("Gameplay tools probe");
    FeatureCheck(std::abs(roundtrip.GetComponent<AudioSourceComponent>(emitter).volume-.4f)<1e-6f,"Audio source undo failed");
    history.Redo(roundtrip); emitter=roundtrip.FindByName("Gameplay tools probe");
    FeatureCheck(std::abs(roundtrip.GetComponent<AudioSourceComponent>(emitter).volume-.8f)<1e-6f,"Audio source redo failed");
    auto copy=SceneSerializer::Duplicate(roundtrip,emitter);
    FeatureCheck(roundtrip.GetComponent<AudioSourceComponent>(copy).clip==source.clip,"Audio source duplicate lost clip");
    auto audio=Audio::GetInstance(); auto sound=Audio::Load("Asset/Sounds/se.mp3");
    FeatureCheck(sound==Audio::Load("Asset/Sounds/../Sounds/se.mp3"),"Audio cache did not reuse canonical path");
    FeatureCheck(!audio->Pause(sound) && audio->Stop(sound),"Unplayed audio null-voice controls failed");
    audio->StopAll();
    ScriptSystem scripts; FeatureCheck(scripts.Initialize(),scripts.Error().c_str());
    scripts.Update(world,.016f); FeatureCheck(scripts.Error().empty(),scripts.Error().c_str());
    FeatureCheck(!world.IsValid(world.FindByName("ScriptCreated")),"Creation mutated ECS inside script callback");
    FeatureCheck(audio->ActiveVoiceCount()==2,"MP3 effects did not overlap independently");
    auto values=nlohmann::json::parse(object->GetComponent<UiElementComponent>().text).get<std::vector<float>>();
    auto q = (Quaternion(Radian(Math::DegreesToRadians(-10)),Vector3::FORWARD) *
              Quaternion(Radian(Math::DegreesToRadians(20)),Vector3::RIGHT) *
              Quaternion(Radian(Math::DegreesToRadians(35)),Vector3::UP)).NormalizeCopy();
    auto rotated = q * Vector3::FORWARD;
    std::vector<float> expected{Math::PI,Math::TwoPI,Math::deg2Rad,Math::sin(.4f),Math::cos(.4f),Math::tan(.4f),
        float(Math::acos(2)),float(Math::asin(-2)),float(Math::atan2(.5f,-.25f)),Math::Lerp(2.f,4.f,1.5f),Math::SmoothStep(-1.f,3.f,1.f),
        Math::Normalize(Vector3::ZERO).x,Math::Normalize(Vector3(3,0,4)).z,Math::Dot(Vector3(1,2,3),Vector3(4,5,6)),
        Math::Cross(Vector3::RIGHT,Vector3::UP).z,std::floor(-1.2f),Math::Equal(1,1.0000001f)?1.f:0.f,
        q.x,q.y,q.z,q.w,rotated.x,rotated.y,Slerp(Quaternion::IDENTITY,Quaternion(Radian(2),Vector3::UP),.3f,true).y,
        Math::Normalize(Vector3(3e20f,0,4e20f)).z,Vector3(1e20f,0,0).Distance(Vector3::ZERO)};
    FeatureCheck(values.size()==expected.size(),"Managed/native math result count differs");
    for(size_t i=0;i<expected.size();++i) FeatureCheck(std::abs(values[i]-expected[i])<=1e-5f*std::max(1.f,std::abs(expected[i])),"Managed/native math parity failed");
    scripts.FlushCommands(world);
    auto created=world.FindByName("ScriptCreated"); FeatureCheck(world.IsValid(created),"Deferred create failed");
    auto child=world.CreateGameObject("Deferred destroy child"); auto childId=child->GetHandle();
    child->AddComponent<TransformComponent>(); child->AddComponent<HierarchyComponent>(world.GetComponent<PersistentIdComponent>(created).value);
    scripts.Update(world,.016f); scripts.FlushCommands(world);
    FeatureCheck(scripts.Error().empty() && !world.IsValid(created) && !world.IsValid(childId),"Deferred subtree destruction failed");
    scripts.Update(world,.016f); scripts.FlushCommands(world);
    FeatureCheck(scripts.Error().empty() && object->GetComponent<TransformComponent>().transition.x==9,"Deferred result, failure or invalid-handle validation failed");
    FeatureCheck(audio->ActiveVoiceCount()==0,"Script StopAudio left voices playing");
    scripts.Reset();
    SceneSerializer::FromXml(world, xml);
    world.GetComponent<ScriptComponent>(world.FindByName("Gameplay tools probe")).scripts[0].fields["RetainForRestart"] = "false";
    scripts.Update(world,.016f);
    FeatureCheck(scripts.Error().empty(),"Old World managed reference became valid after snapshot restore");
    scripts.Reset(); audio->StopAll();
    audio->PlayClip("Asset/Sounds/se.mp3",.2f,true,1234);audio->PlayClip("Asset/Sounds/se.mp3",.2f,true,1234);
    FeatureCheck(audio->ActiveVoiceCount()==1,"Loop owner restart leaked voice");
    audio->SetPaused(true);audio->SetPaused(false);audio->StopAll();
    FeatureCheck(audio->ActiveVoiceCount()==0,"Stop/reset did not clear effects");
    World audioWorld; auto autoplay=audioWorld.CreateGameObject("Auto sound");
    autoplay->AddComponent<TransformComponent>();
    autoplay->AddComponent<AudioSourceComponent>(AudioSourceComponent{"Asset/Sounds/se.mp3",.2f,true,true});
    SceneRuntime runtime; runtime.Update(audioWorld,.016f,true,false);
    FeatureCheck(audio->ActiveVoiceCount()==1,"Audio play-on-awake failed");
    runtime.Update(audioWorld,.016f,true,false,nullptr,true);
    FeatureCheck(audio->IsPaused() && audio->ActiveVoiceCount()==1,"Runtime pause lost audio position");
    runtime.Update(audioWorld,.016f,true,false);
    FeatureCheck(!audio->IsPaused() && audio->ActiveVoiceCount()==1,"Runtime audio resume duplicated voice");
    autoplay->GetComponent<ObjectStateComponent>().activeSelf=false;runtime.Update(audioWorld,.016f,true,false);
    FeatureCheck(audio->ActiveVoiceCount()==1,"Inactive object discarded looping audio state");
    autoplay->GetComponent<ObjectStateComponent>().activeSelf=true;runtime.Update(audioWorld,.016f,true,false);FeatureCheck(audio->ActiveVoiceCount()==1,"Object reenable duplicated looping audio");
    autoplay->GetComponent<AudioSourceComponent>().clip.clear();runtime.Update(audioWorld,.016f,true,false);FeatureCheck(audio->ActiveVoiceCount()==0,"Replacing audio source left previous loop playing");
    audioWorld.DestroyGameObject(autoplay->GetHandle()); runtime.Update(audioWorld,.016f,true,false);
    FeatureCheck(audio->ActiveVoiceCount()==0,"Destroyed audio source left loop playing");
    runtime.Reset();
    WriteLog(LogLevel::Info,"Smoke","Managed/native math parity, queued create/prefab/subtree destroy and MP3 overlap/source XML/pause/stop passed");
}
