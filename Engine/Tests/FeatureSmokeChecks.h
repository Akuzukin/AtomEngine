#pragma once
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Physics/PhysicsWorld.h"
#include "Runtime/Function/Scripting/ScriptSystem.h"
#include "Runtime/Function/Render/SelectionRenderer.h"
#include "Runtime/Resource/Shader.h"
#include "Runtime/Resource/Primitive.h"
#include <fstream>
#include <thread>
#include "Runtime/Function/UI/UiSystem.h"
#include <chrono>
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
inline void FeatureCheck(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
inline void RunFeatureSmokeChecks(AtomEngine::World &world, AtomEngine::Camera &camera)
{
    using namespace AtomEngine;
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto folder = std::filesystem::path(exe).parent_path() / "Smoke";
    std::filesystem::create_directories(folder);
    // Progress, duplicate requests, cached primitives and partial failure must be observable.
    auto resources=SceneSerializer::CollectResources("Asset/Scenes/MaterialTest.scene");
    AssetPreloadBatch preload(resources);
    FeatureCheck(preload.Report().total==3,"Scene dependencies were not deduplicated");
    preload.Step(1);
    FeatureCheck(preload.Report().completed==1 && !preload.Report().Ready(),"Incremental preload budget failed");
    preload.Step(100);
    FeatureCheck(preload.Report().Success(),"Scene resource preload failed");
    auto cached=world.GetComponent<MeshComponent>(world.FindByName("Sphere_0_0")).GetModel();
    FeatureCheck(cached==Primitive::GetModel(BuiltinMesh::Sphere),"Preload did not reuse primitive cache");
    auto failed=AssetManager::Preload({{AssetKind::Texture,"Asset/_MissingPreloadTexture.dds"},
        {AssetKind::Primitive,"",BuiltinMesh::Sphere}});
    FeatureCheck(failed->Report().Ready() && failed->Report().succeeded==1 && failed->Report().errors.size()==1,
                 "Preload did not report partial failure");
    std::string longLog="UTF-8 日志 "+std::string(4096,'x');
    Log("%s",longLog.c_str());
    Log(L"%ls",L"宽字符日志");
    std::thread logThread([] {WriteLog(LogLevel::Info,"Smoke","Worker thread log");}); logThread.join();
    auto history=GetLogHistory();
    bool completeLog=false,wideLog=false,workerLog=false;
    for(auto& entry:history) {completeLog|=entry.message==longLog;wideLog|=entry.message=="宽字符日志";workerLog|=entry.message=="Worker thread log";}
    FeatureCheck(completeLog && wideLog && workerLog,"Log truncation, UTF-8 or concurrent sink regression");
    FlushLog();
    Transform transformed(Vector3(3, 4, 5), Quaternion(Radian(1.2f), Vector3::UP), Vector3(2, 3, 4));
    auto matrix = transformed.GetMatrix();
    auto scale = matrix.GetScale();
    FeatureCheck((scale - Vector3(2, 3, 4)).LengthSqr() < 1e-6f &&
                     std::abs(Quaternion::Dot(matrix.GetRotation(), transformed.rotation)) > .9999f,
                 "Rotated transform scale/rotation decomposition failed");
    FeatureCheck((Math::TransformCoord(Vector3::ZERO, matrix) - transformed.transition).LengthSqr() < 1e-6f,
                 "Point transform lost translation");
    auto sphere = world.FindByName("Sphere_0_0");
    FeatureCheck(world.IsValid(sphere), "Material test scene missing");
    if (!world.HasComponent<ScriptComponent>(sphere))
        world.AddComponent<ScriptComponent>(sphere);
    world.GetComponent<ScriptComponent>(sphere).scripts.clear();
    world.GetComponent<ScriptComponent>(sphere).scripts.push_back(
        {"Scripts/AtomScripts.dll", "Game.Rotate", true, {{"Speed", "1.25"}}});
    auto label = world.GetGameObject(world.FindByName("UI Text"));
    if (!label)
        label = world.CreateGameObject("UI Text");
    if (!label->HasComponent<TransformComponent>())
        label->AddComponent<TransformComponent>();
    if (!label->HasComponent<UiElementComponent>())
        label->AddComponent<UiElementComponent>();
    label->GetComponent<UiElementComponent>().size = Vector2(450, 50);
    label->GetComponent<UiElementComponent>().text = "AtomEngine / C# / 文本渲染";
    auto child = world.CreateGameObject("Child");
    child->AddComponent<TransformComponent>(Vector3(0, 3, 0));
    child->AddComponent<HierarchyComponent>(world.GetComponent<PersistentIdComponent>(sphere).value);
    auto path = folder / "roundtrip.scene";
    SceneSerializer::Save(world, path, "Roundtrip");
    World loaded;
    FeatureCheck(SceneSerializer::Load(loaded, path) == "Roundtrip", "Scene name roundtrip failed");
    auto e = loaded.FindByName("Sphere_0_0");
    FeatureCheck(loaded.GetComponent<ScriptComponent>(e).scripts[0].fields.at("Speed") == "1.25",
                 "Script field roundtrip failed");
    FeatureCheck(loaded.GetComponent<PersistentIdComponent>(e).value ==
                     world.GetComponent<PersistentIdComponent>(sphere).value,
                 "Persistent ID roundtrip failed");
    FeatureCheck(loaded.GetComponent<TransformComponent>(loaded.FindByName("Child")).parent != nullptr,
                 "Hierarchy load failed");
    FeatureCheck(loaded.GetComponent<ColliderComponent>(e).radius == 1, "Collider roundtrip failed");
    FeatureCheck(loaded.GetComponent<UiElementComponent>(loaded.FindByName("UI Text")).text ==
                     "AtomEngine / C# / 文本渲染",
                 "UTF-8 UI roundtrip failed");
    auto prefab = folder / "sphere.prefab";
    SceneSerializer::SavePrefab(world, sphere, prefab);
    auto first = SceneSerializer::InstantiatePrefab(loaded, prefab),
         second = SceneSerializer::InstantiatePrefab(loaded, prefab);
    FeatureCheck(loaded.GetComponent<PersistentIdComponent>(first).value !=
                     loaded.GetComponent<PersistentIdComponent>(second).value,
                 "Prefab instances must have unique IDs");
    // Invalid documents must preserve both entities and existing wrappers.
    auto before = loaded.GetGameObject(e);
    std::ofstream(folder / "invalid.scene")
        << "<Scene version=\"1\"><Object id=\"bad\"><Collider radius=\"-1\"/></Object></Scene>";
    bool rejected = false;
    try
    {
        SceneSerializer::Load(loaded, folder / "invalid.scene");
    }
    catch (...)
    {
        rejected = true;
    }
    FeatureCheck(rejected && before->IsValid(), "Scene load transaction failed");
    auto dependencyFile=folder/"texture-dependencies.scene";
    std::ofstream(dependencyFile) << R"(<Scene version="1"><Object id="texture"><Mesh primitive="3"/><Material><Texture slot="0" path="Asset/Textures/uvChecker.dds"/></Material><UI texture="Asset/Textures/uvChecker.dds"/></Object></Scene>)";
    auto textures=AssetManager::Preload(SceneSerializer::CollectResources(dependencyFile));
    FeatureCheck(textures->Report().Success() && textures->Report().total==2,"Material/UI dependency deduplication failed");
    auto firstTexture=AssetManager::LoadCovertTexture("Asset/Textures/uvChecker.dds",kWhiteOpaque2D,true);
    auto secondTexture=AssetManager::LoadCovertTexture("Asset/Textures/uvChecker.dds",kWhiteOpaque2D,true);
    FeatureCheck(firstTexture.IsValid() && firstTexture.Get()==secondTexture.Get(),"Preloaded texture cache was not reused");
    std::ofstream(dependencyFile) << R"(<Scene version="1"><Object id="missing"><UI texture="Asset/_MissingPreloadTexture.dds"/></Object></Scene>)";
    rejected=false;
    try {SceneSerializer::Load(loaded,dependencyFile);} catch(const std::exception&) {rejected=true;}
    FeatureCheck(rejected && before->IsValid(),"Missing preload dependency replaced the active world");
    ScriptSystem scripts;
    if (!scripts.Initialize())
        throw std::runtime_error(scripts.Error());
    auto original = loaded.GetComponent<TransformComponent>(e).rotation;
    scripts.Update(loaded, .1f);
    scripts.FixedUpdate(loaded, 1.f / 60);
    FeatureCheck(scripts.Error().empty(), scripts.Error().c_str());
    FeatureCheck(std::abs(Quaternion::Dot(original, loaded.GetComponent<TransformComponent>(e).rotation)) < .999f,
                 "C# OnUpdate did not modify GameObject");
    auto button = loaded.FindByName("Click counter");
    FeatureCheck(loaded.IsValid(button) && UiSystem::HitTest(loaded, 30, 90) == button, "UI layer hit test failed");
    scripts.UiClick(loaded, button);
    FeatureCheck(scripts.Error().empty() && loaded.GetComponent<UiElementComponent>(button).text == "Clicks: 1",
                 "C# UI click/text callback failed");
    scripts.Reset();
    World physicsWorld;
    auto ground = physicsWorld.CreateGameObject("Ground");
    ground->AddComponent<TransformComponent>(Vector3(0, -.5f, 0));
    ground->AddComponent<ColliderComponent>();
    ground->GetComponent<ColliderComponent>().halfExtents = Vector3(5, .5f, 5);
    auto falling = physicsWorld.CreateGameObject("Falling");
    falling->AddComponent<TransformComponent>(Vector3(0, 3, 0));
    falling->AddComponent<ColliderComponent>();
    falling->AddComponent<RigidBodyComponent>();
    falling->GetComponent<RigidBodyComponent>().restitution = 0;
    PhysicsWorld physics;
    bool contact = false;
    for (int frame = 0; frame < 180; ++frame)
    {
        physics.Update(physicsWorld, 1.f / 60);
        contact |= !physics.Contacts().empty();
    }
    float y = falling->GetComponent<TransformComponent>().transition.y;
    FeatureCheck(contact && y > .45f && y < .6f, "Jolt falling body/ground collision failed");
    physicsWorld.DestroyGameObject(falling->GetHandle());
    physics.Update(physicsWorld, 1.f / 60);
    auto controlled = physicsWorld.CreateGameObject("Controlled sphere");
    controlled->AddComponent<TransformComponent>(Vector3(20, 3, 0));
    controlled->AddComponent<ColliderComponent>();
    controlled->GetComponent<ColliderComponent>().shape = ColliderShape::Sphere;
    controlled->AddComponent<RigidBodyComponent>();
    controlled->GetComponent<RigidBodyComponent>().gravity = false;
    physics.Update(physicsWorld, 1.f / 60);
    controlled->GetComponent<TransformComponent>().rotation = Quaternion(Radian(.0001f), Vector3::UP);
    physics.Update(physicsWorld, 1.f / 60);
    FeatureCheck(std::abs(controlled->GetComponent<TransformComponent>().rotation.y) > .00001f,
                 "Physics discarded small script rotation updates");
    auto shader = ShaderLibrary::Get(L"Object3D.PS.hlsl", L"ps_6_2", L"main");
    bool specular = false, ao = false;
    for (auto &binding : shader->Bindings())
    {
        specular |= binding.name == "specularTexture" && binding.bindPoint == 6;
        for (auto &var : binding.variables)
            ao |= var.name == "aoFactor" && var.offset == offsetof(MaterialConstants, aoFactor);
    }
    FeatureCheck(specular && ao, "PBR shader reflection/constant buffer layout failed");
    // Recompile a dedicated temporary shader; failed edits retain the last working bytecode.
    auto temp = std::filesystem::path("Asset/Shaders/_AtomFeatureSmoke.hlsl");
    FeatureCheck(!std::filesystem::exists(temp), "Temporary shader already exists");
    struct TemporaryShader
    {
        std::filesystem::path path;
        std::shared_ptr<Shader> shader;
        ~TemporaryShader()
        {
            if (shader)
                ShaderLibrary::Forget(shader);
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } temporary{temp};
    std::ofstream(temp) << "float4 main():SV_TARGET{return float4(1,0,0,1);}";
    temporary.shader = ShaderLibrary::Get(L"_AtomFeatureSmoke.hlsl", L"ps_6_2", L"main");
    auto revision = temporary.shader->Revision();
    auto blob = temporary.shader->Bytecode();
    std::ofstream(temp) << "invalid shader syntax";
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    FeatureCheck(!ShaderLibrary::PollChanges() && !ShaderLibrary::Error().empty() &&
                     temporary.shader->Revision() == revision && temporary.shader->Bytecode() == blob,
                 "Failed shader reload did not retain last working shader");
    std::ofstream(temp) << "float4 main():SV_TARGET{return float4(0,1,0,1);}";
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    FeatureCheck(ShaderLibrary::PollChanges() && ShaderLibrary::Error().empty() &&
                     temporary.shader->Revision() > revision,
                 "Valid shader reload did not update revision");
    auto &pickContext = GraphicsContext::Begin(L"Picking regression");
    for (auto entity : world.View<MeshComponent, TransformComponent, MaterialComponent>())
        world.GetComponent<MeshComponent>(entity).Update(pickContext, world.GetComponent<TransformComponent>(entity),
                                                         world.GetComponent<MaterialComponent>(entity), 0);
    pickContext.Finish();
    auto center = world.GetComponent<TransformComponent>(sphere).GetWorldPosition();
    auto clip = Vector4(center.x, center.y, center.z, 1) * camera.GetViewProjMatrix();
    float width = float(gSceneColorBuffer.GetWidth()), height = float(gSceneColorBuffer.GetHeight());
    auto picked = SelectionRenderer::Pick(world, camera, (clip.x / clip.w + 1) * width * .5f,
                                          (1 - clip.y / clip.w) * height * .5f, width, height);
    if (picked != sphere)
    {
        auto name = world.TryGet<NameComponent>(picked);
        throw std::runtime_error(std::string("Viewport picking: ") + (name ? name->value : "None") +
                                 " center=" + std::to_string(center.x) + "," + std::to_string(center.y) + "," +
                                 std::to_string(center.z));
    }
    SelectionRenderer::Select(&world, sphere);
    std::ofstream(folder / "features-passed.txt")
        << "Scene XML, hierarchy, prefab IDs, invalid scene rollback, C# lifecycle, Jolt collision, PBR reflection and "
           "shader reload rollback passed\n";
}
