#include "Runtime/Function/Render/Particle/ParticleEmitterRuntime.h"
#include "Runtime/Function/Render/Particle/ParticleComponentEditor.h"
#include "Runtime/Function/Render/Particle/ParticleAsset.h"
#pragma once
#include "GameplayToolsSmokeChecks.h"
#include "Runtime/Function/UI/UiSystem.h"
#include "Runtime/Function/Scene/SceneAuthoring.h"
inline void RunEngineCompletionSmokeChecks() {
    using namespace AtomEngine;
    {
        World timed;ScriptSystem clock;
        auto first=timed.CreateGameObject("Time first"),second=timed.CreateGameObject("Time second");
        for(auto object:{first.get(),second.get()}){object->AddComponent<TransformComponent>();object->AddComponent<UiElementComponent>();object->AddComponent<ScriptComponent>();object->GetComponent<ScriptComponent>().scripts.push_back({"Scripts/AtomScripts.dll","Game.TimeProbe",true,{}});}
        auto read=[&]{return nlohmann::json::parse(first->GetComponent<UiElementComponent>().text);};
        clock.Update(timed,.04f);auto frame=read();FeatureCheck(clock.Error().empty(),clock.Error().c_str());
        FeatureCheck(frame["frames"]==1 && frame["fixedFrames"]==0 && std::abs(frame["elapsed"].get<double>()-.04)<1e-6 && std::abs(frame["fps"].get<float>()-25)<1e-4,"Initial game clock incorrect");
        FeatureCheck(first->GetComponent<UiElementComponent>().text==second->GetComponent<UiElementComponent>().text,"Clock advanced per script instead of per frame");
        clock.FixedUpdate(timed,1.f/60);clock.FixedUpdate(timed,1.f/60);auto fixed=read();
        FeatureCheck(clock.Error().empty(),clock.Error().c_str());FeatureCheck(fixed["frames"]==1 && fixed["fixedFrames"]==2 && fixed["fixedStep"]==true && std::abs(fixed["fixedElapsed"].get<double>()-1./30)<1e-6,"Multiple fixed steps counted incorrectly");
        clock.UiClick(timed,first->GetHandle());FeatureCheck(read()["fixedStep"]==false && std::abs(read()["delta"].get<float>()-.04f)<1e-6,"Fixed callback time leaked into UI callback");
        clock.PauseTime();clock.UiClick(timed,first->GetHandle());auto paused=read();FeatureCheck(paused["paused"]==true && paused["delta"]==0 && paused["fps"]==0 && paused["frames"]==1 && paused["elapsed"]==frame["elapsed"],"Pause advanced game clock");
        wchar_t clockExe[32768]{};GetModuleFileNameW(nullptr,clockExe,DWORD(std::size(clockExe)));auto path=std::filesystem::path(clockExe).parent_path()/"Scripts/AtomScripts.dll";
        FeatureCheck(clock.ReloadAssembly(path),clock.Error().c_str());FeatureCheck(read()==paused,"Hot reload reset game clock");
        clock.Update(timed,.02f);FeatureCheck(read()["frames"]==2 && std::abs(read()["elapsed"].get<double>()-.06)<1e-6 && read()["paused"]==false,"Resume clock incorrect");
        clock.Reset();clock.Update(timed,.01f);FeatureCheck(read()["frames"]==1 && read()["fixedFrames"]==0 && std::abs(read()["elapsed"].get<double>()-.01)<1e-6,"Play clock did not reset");clock.Reset();
        SceneRuntime clockRuntime;clockRuntime.Update(timed,.04f,true,false);clockRuntime.Update(timed,5.f,true,false,nullptr,true);clockRuntime.Update(timed,.02f,true,false);
        FeatureCheck(read()["frames"]==2 && std::abs(read()["elapsed"].get<double>()-.06)<1e-6,"Scene pause included suspended time");clockRuntime.Reset();
        WriteLog(LogLevel::Info,"Smoke","Shared frame clock, fixed steps, callback time, pause/resume, reload and Play reset passed");
    }
    ScriptSystem scripts;auto schema=nlohmann::json::parse(scripts.DescribeAssembly("Scripts/AtomScripts.dll"));
    bool audioReflected=false,keyReflected=false;for(auto &type:schema)if(type["name"]=="Game.TypedApiProbe")for(auto &field:type["fields"]){if(field["name"]=="Clip" && field["kind"]=="audioClip")audioReflected=true;if(field["name"]=="FireKey" && field["kind"]=="enum")for(auto &choice:field["choices"])if(choice=="Space")keyReflected=true;}
    FeatureCheck(audioReflected && keyReflected,"Typed input/audio field metadata missing");
    ScriptAttachment typedDefinition{"Scripts/AtomScripts.dll","Game.TypedApiProbe",true,{{"FireKey","F"},{"Controls","ArrowKeys"},{"Clip","Asset/Sounds/se.mp3"}}};
    FeatureCheck(scripts.ValidateFields(typedDefinition),scripts.Error().c_str());auto invalidClip=typedDefinition;invalidClip.fields["Clip"]="bad.txt";FeatureCheck(!scripts.ValidateFields(invalidClip),"Invalid typed audio field accepted");
    World typedWorld;auto typedObject=typedWorld.CreateGameObject("Typed input/audio probe");typedObject->AddComponent<TransformComponent>();typedObject->AddComponent<UiElementComponent>();typedObject->AddComponent<ScriptComponent>();typedObject->GetComponent<ScriptComponent>().scripts.push_back(typedDefinition);
    auto input=Input::GetInstance();auto previousInput=input->GameplayInputEnabled();input->SetGameplayInputEnabled(false);
    ScriptSystem typedScripts;typedScripts.Update(typedWorld,.016f);FeatureCheck(typedScripts.Error().empty(),typedScripts.Error().c_str());FeatureCheck(Audio::GetInstance()->ActiveVoiceCount()==1,"Typed audio did not create native voice");
    FeatureCheck(typedObject->GetComponent<UiElementComponent>().text=="1:F:ArrowKeys:Asset/Sounds/se.mp3","Typed fields were not applied");
    auto &typedFields=typedObject->GetComponent<ScriptComponent>().scripts[0].fields;typedFields["FireKey"]="Space";typedFields["Clip"]="";typedScripts.Update(typedWorld,.016f);
    FeatureCheck(typedObject->GetComponent<UiElementComponent>().text=="2:Space:ArrowKeys:","Live key/audio edit recreated script or failed");
    typedScripts.Reset();input->SetGameplayInputEnabled(previousInput);FeatureCheck(Audio::GetInstance()->ActiveVoiceCount()==0,"Typed audio owner cleanup failed");
    auto typedXml=SceneSerializer::ToXml(typedWorld,"Typed fields");World typedCopy;SceneSerializer::FromXml(typedCopy,typedXml);FeatureCheck(typedCopy.GetComponent<ScriptComponent>(typedCopy.FindByName("Typed input/audio probe")).scripts[0].fields.at("FireKey")=="Space","Typed field XML roundtrip failed");
    bool reflected=false;for(auto &type:schema)if(type["name"]=="Game.CompletionProbe")for(auto &f:type["fields"])if(f["name"]=="Offset" && f["kind"]=="vector3")reflected=true;
    FeatureCheck(reflected,"Script metadata/vector reflection missing");
    ScriptAttachment definition{"Scripts/AtomScripts.dll","Game.CompletionProbe",true,{{"Offset","[7,8,9]"},{"Mode","Second"}}};
    FeatureCheck(scripts.ValidateFields(definition),scripts.Error().c_str());
    auto bad=definition;bad.fields["Missing"]="3";FeatureCheck(!scripts.ValidateFields(bad),"Unknown script field accepted");
    bad=definition;bad.fields["Offset"]="[1,2]";FeatureCheck(!scripts.ValidateFields(bad),"Invalid vector accepted");
    bad=definition;bad.fields["Mode"]="999";FeatureCheck(!scripts.ValidateFields(bad),"Invalid enum accepted");
    bad=definition;bad.fields["Speed"]="NaN";FeatureCheck(!scripts.ValidateFields(bad),"Nonfinite field accepted");
    World world;auto probe=world.CreateGameObject("Completion probe");probe->AddComponent<TransformComponent>();probe->AddComponent<UiElementComponent>();probe->AddComponent<ScriptComponent>();probe->GetComponent<ScriptComponent>().scripts.push_back(definition);
    auto target=world.CreateGameObject("Query box");target->AddComponent<TransformComponent>();target->GetComponent<TransformComponent>().transition=Vector3(5,0,0);target->AddComponent<ColliderComponent>();target->GetComponent<ColliderComponent>().collisionLayer=2;
    PhysicsWorld physics;PhysicsHit hit;
    FeatureCheck(physics.Raycast(world,Vector3::ZERO,Vector3::RIGHT,20,hit,1u<<2) && hit.entity==target->GetHandle() && std::abs(hit.distance-4.5f)<.01f,"Native precise ray/filter failed");
    FeatureCheck(!physics.Raycast(world,Vector3::ZERO,Vector3::RIGHT,20,hit,1u<<1),"Ray mask ignored");
    target->GetComponent<ColliderComponent>().trigger=true;FeatureCheck(!physics.Raycast(world,Vector3::ZERO,Vector3::RIGHT,20,hit),"Trigger filter ignored");
    FeatureCheck(physics.Raycast(world,Vector3::ZERO,Vector3::RIGHT,20,hit,~0u,true),"Included trigger missing");
    FeatureCheck(!physics.Raycast(world,Vector3::ZERO,Vector3::RIGHT,20,hit,~0u,true,target->GetHandle()),"Ignored body returned");
    target->GetComponent<ColliderComponent>().trigger=false;
    scripts.SetPhysics(&physics);scripts.Update(world,.016f);FeatureCheck(scripts.Error().empty(),scripts.Error().c_str());
    FeatureCheck(probe->GetComponent<UiElementComponent>().text=="1:2:7:Second","Typed vector/enum overrides not applied");
    auto parent=world.CreateGameObject("Inactive parent");parent->AddComponent<TransformComponent>();
    probe->AddComponent<HierarchyComponent>(parent->GetComponent<PersistentIdComponent>().value);
    parent->GetComponent<ObjectStateComponent>().activeSelf=false;
    scripts.Update(world,.016f);FeatureCheck(!world.IsActive(probe->GetHandle()) && probe->GetComponent<UiElementComponent>().text=="1:2:7:Second","Inactive subtree still updated");
    parent->GetComponent<ObjectStateComponent>().activeSelf=true;
    scripts.Update(world,.016f);FeatureCheck(probe->GetComponent<UiElementComponent>().text=="2:2:7:Second","Reenable recreated script instance");
    world.SetComponentEnabled(probe->GetHandle(),ComponentKind::Script,false);scripts.Update(world,.016f);
    FeatureCheck(probe->GetComponent<UiElementComponent>().text=="2:2:7:Second","Disabled script component still updated");
    world.SetComponentEnabled(probe->GetHandle(),ComponentKind::Script,true);scripts.Update(world,.016f);
    FeatureCheck(probe->GetComponent<UiElementComponent>().text=="3:2:7:Second","Component reenable lost instance state");
    target->GetComponent<ObjectStateComponent>().activeSelf=false;
    FeatureCheck(!physics.Raycast(world,Vector3::ZERO,Vector3::RIGHT,20,hit),"Inactive collider participates in query");
    target->GetComponent<ObjectStateComponent>().activeSelf=true;world.SetComponentEnabled(target->GetHandle(),ComponentKind::Collider,false);
    FeatureCheck(physics.OverlapSphere(world,Vector3(5,0,0),1).empty(),"Disabled collider overlaps");
    world.SetComponentEnabled(target->GetHandle(),ComponentKind::Collider,true);
    FeatureCheck(physics.OverlapSphere(world,Vector3(5,0,0),1).size()==1,"Reenabled collider missing");
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));auto assembly=std::filesystem::path(exe).parent_path()/"Scripts/AtomScripts.dll";
    FeatureCheck(scripts.ReloadAssembly(assembly),scripts.Error().c_str());scripts.Update(world,.016f);
    FeatureCheck(probe->GetComponent<UiElementComponent>().text=="4:2:7:Second","Explicit private reload state lost");
    auto &parameters=probe->GetComponent<ScriptComponent>().scripts[0].fields;
    parameters["Speed"]="5";scripts.Update(world,.016f);FeatureCheck(probe->GetComponent<UiElementComponent>().text=="5:5:7:Second","Live parameter edit recreated script or failed");
    auto values=nlohmann::json::parse(scripts.ReadFields(world,probe->GetHandle(),0));FeatureCheck(values["Speed"]=="5","Live script value read failed");
    parameters.erase("Speed");scripts.Update(world,.016f);FeatureCheck(probe->GetComponent<UiElementComponent>().text=="6:2:7:Second","Removing override failed to restore constructor default");
    parameters["Speed"]="NaN";scripts.Update(world,.016f);FeatureCheck(probe->GetComponent<UiElementComponent>().text=="7:2:7:Second" && !parameters.contains("Speed"),"Invalid live parameter changed instance or was retained");
    parameters["FailRestore"]="true";scripts.Update(world,.016f);FeatureCheck(!scripts.ReloadAssembly(assembly),"Failed state restoration committed a reload");
    scripts.Update(world,.016f);FeatureCheck(probe->GetComponent<UiElementComponent>().text=="9:2:7:Second","Failed reload discarded previous instance");
    scripts.Reset();
    World spawned;auto spawner=spawned.CreateGameObject("Prefab apply probe");spawner->AddComponent<TransformComponent>();spawner->AddComponent<UiElementComponent>();spawner->AddComponent<ScriptComponent>();spawner->GetComponent<ScriptComponent>().scripts.push_back({"Scripts/AtomScripts.dll","Game.CompletionSpawnProbe",true,{}});
    ScriptSystem spawning;spawning.Update(spawned,.016f);FeatureCheck(!spawned.IsValid(spawned.FindByName("Script prefab target")),"Create applied during callback");spawning.FlushCommands(spawned);
    auto targetHandle=spawned.FindByName("Script prefab target");auto targetId=spawned.GetComponent<PersistentIdComponent>(targetHandle).value;
    spawning.Update(spawned,.016f);FeatureCheck(!spawned.HasComponent<ColliderComponent>(targetHandle),"ApplyPrefab mutated ECS during callback");spawning.FlushCommands(spawned);
    spawning.Update(spawned,.016f);FeatureCheck(spawning.Error().empty(),spawning.Error().c_str());
    FeatureCheck(spawner->GetComponent<UiElementComponent>().text=="attached" && spawned.GetComponent<PersistentIdComponent>(targetHandle).value==targetId && spawned.HasComponent<MeshComponent>(targetHandle) && spawned.HasComponent<RigidBodyComponent>(targetHandle),"Script-created GameObject did not receive prefab properties");
    spawning.Reset();
    auto xml=SceneSerializer::ToXml(world,"query");World copy;SceneSerializer::FromXml(copy,xml);
    FeatureCheck(copy.GetComponent<ColliderComponent>(copy.FindByName("Query box")).collisionLayer==2,"Collision layer XML lost");
    auto panel=world.CreateGameObject("Anchored panel");panel->AddComponent<TransformComponent>();panel->AddComponent<UiElementComponent>();auto &panelUI=panel->GetComponent<UiElementComponent>();
    panelUI.responsive=true;panelUI.anchorMin=panelUI.anchorMax={1,1};panelUI.pivot={1,1};panelUI.position={-20,-20};panelUI.size={300,100};panelUI.layout=UiLayout::Horizontal;
    world.uiViewportSize={1280,720};world.uiRenderSize={1920,1080};auto rect=UiSystem::Resolve(world,panel->GetHandle());
    FeatureCheck(std::abs(rect.position.x-1600)<.01f && std::abs(rect.position.y-960)<.01f,"Responsive anchors at 16:9 failed");
    world.uiViewportSize={800,600};rect=UiSystem::Resolve(world,panel->GetHandle());FeatureCheck(rect.position.x>=0 && rect.position.y>=0 && rect.position.x+rect.size.x<=1920,"Anchored panel escaped 4:3 viewport");
    auto first=world.CreateGameObject("UI first");first->AddComponent<TransformComponent>();first->AddComponent<HierarchyComponent>(panel->GetComponent<PersistentIdComponent>().value);first->AddComponent<UiElementComponent>();auto &firstUI=first->GetComponent<UiElementComponent>();firstUI.responsive=true;firstUI.interactable=true;firstUI.size={100,40};firstUI.layer=10;
    auto second=world.CreateGameObject("UI second");second->AddComponent<TransformComponent>();second->AddComponent<HierarchyComponent>(panel->GetComponent<PersistentIdComponent>().value);second->AddComponent<UiElementComponent>();auto &secondUI=second->GetComponent<UiElementComponent>();secondUI.responsive=true;secondUI.interactable=true;secondUI.size={100,40};secondUI.layoutOrder=1;secondUI.layer=10;
    auto a=UiSystem::Resolve(world,first->GetHandle()),b=UiSystem::Resolve(world,second->GetHandle());FeatureCheck(b.position.x>a.position.x+a.size.x,"Horizontal layout spacing failed");
    FeatureCheck(UiSystem::HitTest(world,b.position.x+2,b.position.y+2)==second->GetHandle(),"Resolved layout hit testing failed");
    FeatureCheck(UiSystem::Navigate(world,0)==first->GetHandle() && UiSystem::Navigate(world,1)==second->GetHandle() && UiSystem::Navigate(world,1)==first->GetHandle(),"UI focus wrap failed");
    world.GetComponent<ObjectStateComponent>(second->GetHandle()).activeSelf=false;FeatureCheck(UiSystem::Navigate(world,1)==first->GetHandle(),"Inactive UI received focus");
    World pool;auto original=pool.CreateGameObject("Pool source");original->AddComponent<TransformComponent>();auto originalId=original->GetComponent<PersistentIdComponent>().value;
    auto child=pool.CreateGameObject("Pool child");child->AddComponent<TransformComponent>();child->AddComponent<HierarchyComponent>(originalId);
    EditorDocument document;document.Reset(pool,"Pool");auto objects=SceneAuthoring::CreatePool(pool,original->GetHandle(),4,2,Vector3(3,0,4),true);
    FeatureCheck(objects.size()==4 && pool.View<PersistentIdComponent>().size()==10,"Pool subtree count failed");
    std::set<std::string> unique;for(auto e:pool.View<PersistentIdComponent>())FeatureCheck(unique.insert(pool.GetComponent<PersistentIdComponent>(e).value).second,"Pool IDs not unique");
    for(auto e:objects)FeatureCheck(!pool.IsActive(e),"Pool did not start inactive");
    FeatureCheck(pool.GetComponent<TransformComponent>(objects[3]).transition==Vector3(3,0,4),"Pool grid spacing failed");
    document.Capture(pool,"Pool",originalId,"Create object pool");document.Undo(pool);FeatureCheck(pool.View<PersistentIdComponent>().size()==2,"Pool undo not atomic");document.Redo(pool);FeatureCheck(pool.View<PersistentIdComponent>().size()==10,"Pool redo failed");
    auto intact=SceneSerializer::ToXml(pool,"Pool");bool rejected=false;
    try{SceneAuthoring::CreatePool(pool,entt::null,3,2,Vector3(1,0,1),true,"missing.pool.prefab");}catch(...){rejected=true;}
    FeatureCheck(rejected && SceneSerializer::ToXml(pool,"Pool")==intact,"Failed batch modified scene");
    auto blank=pool.CreateGameObject("Keep target");blank->AddComponent<TransformComponent>();blank->GetComponent<TransformComponent>().transition={8,4,2};auto blankId=blank->GetComponent<PersistentIdComponent>().value;
    auto attachPrefab=std::filesystem::path(exe).parent_path()/"Smoke/CompletionAttach.prefab";std::filesystem::create_directories(attachPrefab.parent_path());SceneSerializer::SavePrefab(pool,pool.FindById(originalId),attachPrefab);
    SceneSerializer::AttachPrefab(pool,blank->GetHandle(),attachPrefab);FeatureCheck(blank->GetComponent<PersistentIdComponent>().value==blankId && blank->GetComponent<TransformComponent>().transition==Vector3(8,4,2),"Native attach changed target identity/pose");
    bool attachedChild=false;for(auto e:pool.View<HierarchyComponent>())if(pool.GetComponent<HierarchyComponent>(e).parentId==blankId)attachedChild=true;FeatureCheck(attachedChild,"Prefab children not attached to existing target");
    auto untouched=pool.CreateGameObject("Bad attach target");untouched->AddComponent<TransformComponent>();auto count=pool.View<PersistentIdComponent>().size();bool invalid=false;try{SceneSerializer::AttachPrefab(pool,untouched->GetHandle(),"missing.prefab");}catch(...){invalid=true;}FeatureCheck(invalid && pool.IsValid(untouched->GetHandle()) && pool.View<PersistentIdComponent>().size()==count && !pool.HasComponent<PrefabInstanceComponent>(untouched->GetHandle()),"Failed attach mutated target");
    World filtered;auto stationary=filtered.CreateGameObject("Stationary");stationary->AddComponent<TransformComponent>();stationary->AddComponent<ColliderComponent>();stationary->GetComponent<ColliderComponent>().collisionLayer=2;stationary->GetComponent<ColliderComponent>().collisionMask=1u<<3;
    auto moving=filtered.CreateGameObject("Moving sensor");moving->AddComponent<TransformComponent>();moving->AddComponent<ColliderComponent>();moving->GetComponent<ColliderComponent>().collisionLayer=3;moving->GetComponent<ColliderComponent>().collisionMask=0;moving->GetComponent<ColliderComponent>().trigger=true;moving->AddComponent<RigidBodyComponent>();moving->GetComponent<RigidBodyComponent>().gravity=false;
    PhysicsWorld filteredPhysics;filteredPhysics.Update(filtered,1.f/60);FeatureCheck(filteredPhysics.Contacts().empty(),"Collision pair mask ignored");moving->GetComponent<ColliderComponent>().collisionMask=1u<<2;filteredPhysics.Update(filtered,1.f/60);FeatureCheck(!filteredPhysics.Contacts().empty(),"Allowed collision pair did not contact");
    World templated;SceneSerializer::FromXml(templated,SceneSerializer::ToXml(pool,"Template fixture"));document.Reset(templated,"Template copy");document.MarkUnsaved();FeatureCheck(document.Dirty(),"Template copy marked saved");
    {
        World particles;auto a=particles.CreateGameObject("Emitter A"),b=particles.CreateGameObject("Emitter B");
        a->AddComponent<TransformComponent>();b->AddComponent<TransformComponent>();a->AddComponent<ParticleComponent>();b->AddComponent<ParticleComponent>();
        a->GetComponent<ParticleComponent>().settings.duration=.1f;a->GetComponent<ParticleComponent>().settings.rate=8;
        EmitSceneParticle(particles,a->GetHandle(),Vector3::ZERO);EmitSceneParticle(particles,b->GetHandle(),Vector3(1,0,0));
        auto first=a->GetComponent<ParticleComponent>().runtimeId,second=b->GetComponent<ParticleComponent>().runtimeId;
        ParticleSystem::StopParticle(first);FeatureCheck(ParticleSystem::GetCurrentLife(second)>=0,"Particle removal invalidated another emitter handle");
        EmitSceneParticle(particles,a->GetHandle(),Vector3(2,0,0));FeatureCheck(a->GetComponent<ParticleComponent>().runtimeId==first,"Particle replay allocated another emitter");
        ParticleSystem::ReleaseParticle(first);ParticleSystem::ReleaseParticle(second);
        a->GetComponent<ParticleComponent>().runtimeId=0xffffffff;b->GetComponent<ParticleComponent>().runtimeId=0xffffffff;
        World saved;SceneSerializer::FromXml(saved,SceneSerializer::ToXml(particles,"Particles"));auto &copy=saved.GetComponent<ParticleComponent>(saved.FindByName("Emitter A"));
        FeatureCheck(copy.settings.duration==.1f&&copy.settings.rate==8&&copy.runtimeId==0xffffffff,"Particle component XML roundtrip persisted runtime handle");
        auto bId=b->GetComponent<PersistentIdComponent>().value;
        a->AddComponent<UiElementComponent>();a->AddComponent<ScriptComponent>();
        a->GetComponent<ScriptComponent>().scripts.push_back({"Scripts/AtomScripts.dll","Game.ParticleApiProbe",true,{{"Effect","particle:"+bId}}});
        ScriptSystem refs;refs.Update(particles,.016f);FeatureCheck(refs.Error().empty(),refs.Error().c_str());
        FeatureCheck(a->GetComponent<UiElementComponent>().text==bId,"Particle script reference not bound before OnCreate");
        auto &fields=a->GetComponent<ScriptComponent>().scripts[0].fields;
        fields["Effect"]="";refs.Update(particles,.016f);FeatureCheck(a->GetComponent<UiElementComponent>().text=="unassigned","Live particle reference clear failed");
        fields["Effect"]="particle:"+bId;refs.Update(particles,.016f);
        FeatureCheck(refs.ReloadAssembly(std::filesystem::path(exe).parent_path()/"Scripts/AtomScripts.dll"),refs.Error().c_str());
        refs.Update(particles,.016f);FeatureCheck(a->GetComponent<UiElementComponent>().text==bId,"Reload lost particle reference world binding");refs.Reset();
        ParticleSystem::ReleaseParticle(b->GetComponent<ParticleComponent>().runtimeId);b->GetComponent<ParticleComponent>().runtimeId=0xffffffff;
        b->AddComponent<HierarchyComponent>();b->GetComponent<HierarchyComponent>().parentId=a->GetComponent<PersistentIdComponent>().value;
        auto particlePrefab=std::filesystem::path(exe).parent_path()/"Smoke/Particles.prefab";SceneSerializer::SavePrefab(particles,a->GetHandle(),particlePrefab);
        World prefabWorld;auto prefabRoot=SceneSerializer::InstantiatePrefab(prefabWorld,particlePrefab);
        auto assigned=prefabWorld.GetComponent<ScriptComponent>(prefabRoot).scripts[0].fields.at("Effect").substr(9);
        FeatureCheck(assigned!=bId&&prefabWorld.HasComponent<ParticleComponent>(prefabWorld.FindById(assigned)),"Prefab particle component reference was not remapped");
        auto dropped=SceneSerializer::InstantiateParticlePrefab(prefabWorld,particlePrefab);
        FeatureCheck(prefabWorld.HasComponent<ParticleComponent>(dropped),"Particle prefab field drop did not resolve component");
        auto beforeDrop=prefabWorld.View<PersistentIdComponent>().size();bool wrongPrefab=false;
        try{SceneSerializer::InstantiateParticlePrefab(prefabWorld,attachPrefab);}catch(...){wrongPrefab=true;}
        FeatureCheck(wrongPrefab&&prefabWorld.View<PersistentIdComponent>().size()==beforeDrop,"Invalid particle prefab field drop changed scene");
        ParticleSettings lowRate;lowRate.capacity=64;lowRate.burst=0;lowRate.rate=10;lowRate.duration=2;lowRate.lifetime=4;
        auto handle=ParticleSystem::Create(lowRate,true);ParticleSystem::EmitParticle(handle,Vector3::ZERO);
        {auto &gpu=GraphicsContext::Begin(L"Fractional emission regression");for(int i=0;i<240;i++)ParticleSystem::UpdatePreview(gpu.GetComputeContext(),handle,1.f/240);gpu.Finish(true);}
        auto alive=ParticleSystem::ReadAliveCountForDiagnostics(handle);FeatureCheck(alive>=9&&alive<=11,"Fractional GPU emission disappeared at high frame rate");
        ParticleSystem::ReleaseParticle(handle);auto replacement=ParticleSystem::Create(lowRate,true);FeatureCheck(handle!=replacement&&ParticleSystem::GetCurrentLife(handle)<0,"Reused particle slot accepted stale handle");ParticleSystem::ReleaseParticle(replacement);
        ParticleComponentEditor::Open(saved,saved.FindByName("Emitter B"));
        for(int frame=0;frame<20;frame++)ParticleComponentEditor::Render(1.f/60);
        ParticleComponentEditor::CapturePreview(std::filesystem::path(exe).parent_path()/"Smoke/ParticlePreview.bin");ParticleComponentEditor::Shutdown();
        for(int shape=0;shape<5;shape++) {
            ParticleSettings burstOnly;burstOnly.capacity=64;burstOnly.burst=10;burstOnly.rate=0;burstOnly.lifetime=.1f;burstOnly.shape=EmitterShape(shape);
            auto isolated=ParticleSystem::Create(burstOnly,true);ParticleSystem::EmitParticle(isolated,Vector3::ZERO);
            {auto &gpu=GraphicsContext::Begin(L"Preview isolation");ParticleSystem::Update(gpu.GetComputeContext(),1.f/60);gpu.Finish(true);}
            FeatureCheck(ParticleSystem::ReadAliveCountForDiagnostics(isolated)==0,"Preview emitter advanced in scene simulation");
            {auto &gpu=GraphicsContext::Begin(L"Emitter shape");ParticleSystem::UpdatePreview(gpu.GetComputeContext(),isolated,1.f/60);gpu.Finish(true);}
            FeatureCheck(ParticleSystem::ReadAliveCountForDiagnostics(isolated)==10,"Emitter shape did not generate requested burst");
            ParticleSystem::StopParticle(isolated);
            {auto &gpu=GraphicsContext::Begin(L"Particle natural expiry");for(int frame=0;frame<18;frame++)ParticleSystem::UpdatePreview(gpu.GetComputeContext(),isolated,1.f/60);gpu.Finish(true);}
            FeatureCheck(ParticleSystem::ReadAliveCountForDiagnostics(isolated)==0,"Stopped particles did not expire");ParticleSystem::ReleaseParticle(isolated);
        }
        auto previewEntity=saved.FindByName("Emitter B");
        for(int blend=0;blend<4;blend++)for(int billboard=0;billboard<2;billboard++) {
            auto &config=saved.GetComponent<ParticleComponent>(previewEntity).settings;config.blend=ParticleBlend(blend);config.billboard=billboard!=0;config.texture=billboard?"Asset/Textures/Particle.png":"";
            ParticleComponentEditor::Open(saved,previewEntity);for(int frame=0;frame<20;frame++)ParticleComponentEditor::Render(1.f/60);
            ParticleComponentEditor::CapturePreview(std::filesystem::path(exe).parent_path()/("Smoke/ParticleBlend"+std::to_string(blend)+"-"+std::to_string(billboard)+".bin"));ParticleComponentEditor::Shutdown();
        }
        auto particleDirectory=std::filesystem::path(exe).parent_path()/"Smoke/ParticleAssets";
        std::filesystem::create_directories(particleDirectory);
        auto exportPath=particleDirectory/L"粒子 导出.particle";
        auto &exportComponent=saved.GetComponent<ParticleComponent>(previewEntity);exportComponent.settings.rate=37;
        ParticleComponentEditor::Open(saved,previewEntity);ParticleComponentEditor::SaveAs(exportPath);
        FeatureCheck(LoadParticleAsset(exportPath).rate==37&&!exportComponent.preset.empty(),"Component .particle export lost settings or binding");
        auto originalFile=ReadAssetText(exportPath);bool invalidParticle=false;auto invalidSettings=exportComponent.settings;invalidSettings.rate=std::numeric_limits<float>::infinity();
        try{SaveParticleAsset(exportPath,invalidSettings);}catch(...){invalidParticle=true;}
        FeatureCheck(invalidParticle&&ReadAssetText(exportPath)==originalFile,"Invalid asset save damaged existing particle file");
        invalidParticle=false;try{SaveParticleAsset(particleDirectory/"wrong.json",exportComponent.settings);}catch(...){invalidParticle=true;}
        FeatureCheck(invalidParticle&&!std::filesystem::exists(particleDirectory/"wrong.json"),"Particle save accepted wrong extension");
        auto sceneBeforeAsset=SceneSerializer::ToXml(saved,"Particle asset isolation");
        auto createdAsset=ParticleComponentEditor::CreateAsset(particleDirectory);
        auto secondAsset=ParticleComponentEditor::CreateAsset(particleDirectory);
        FeatureCheck(createdAsset!=secondAsset&&createdAsset.extension()==L".particle","Particle creation overwrote another asset");
        ParticleComponentEditor::OpenAsset(exportPath);
        for(int frame=0;frame<10;frame++)ParticleComponentEditor::Render(1.f/60);
        auto savedAs=particleDirectory/"SavedAs.particle";ParticleComponentEditor::SaveAs(savedAs);
        FeatureCheck(LoadParticleAsset(savedAs).rate==37&&!ParticleComponentEditor::HasUnsavedChanges(),"Asset editor reopen/save-as lost data");
        ParticleComponentEditor::CapturePreview(particleDirectory/"AssetPreview.bin");ParticleComponentEditor::Shutdown();
        FeatureCheck(SceneSerializer::ToXml(saved,"Particle asset isolation")==sceneBeforeAsset,"Asset editor altered scene objects");
        World particleAssetWorld;auto assetInstance=SceneSerializer::InstantiateParticlePrefab(particleAssetWorld,exportPath);
        auto &bound=particleAssetWorld.GetComponent<ParticleComponent>(assetInstance);
        FeatureCheck(bound.settings.rate==37&&bound.preset==WStringToUTF8(exportPath.generic_wstring()),"Particle asset field drop did not bind native component");
        EmitSceneParticle(particleAssetWorld,assetInstance,Vector3::ZERO);ParticleSystem::ReleaseParticle(bound.runtimeId);
        WriteLog(LogLevel::Info,"Smoke","Particle v2 GPU emission, component references and .particle creation/export/reopen/atomic save/scene isolation/asset instantiation passed");
        auto audio=Audio::GetInstance();auto master=audio->GetMasterVolume(),bus=audio->GetBusVolume(1);audio->SetMasterVolume(.4f);audio->SetBusVolume(1,.3f);
        FeatureCheck(audio->GetMasterVolume()==.4f&&audio->GetBusVolume(1)==.3f,"Audio mixer state failed");audio->SetMasterVolume(master);audio->SetBusVolume(1,bus);
    }
    WriteLog(LogLevel::Info,"Smoke","Reflection, live fields, query/collision filters, Active, UI layout/navigation, reload state, templates/pools and prefab attachment passed");
}
