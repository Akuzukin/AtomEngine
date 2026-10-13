#pragma once
#include "Runtime/Function/Physics/CollisionGeometry.h"
#include "Runtime/Function/Render/RenderGraph.h"
#include "Runtime/Function/Render/PostEffect/FXAA.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Editor/ScriptTools.h"
#include <thread>
inline void RunEnhancementSmokeChecks()
{
    using namespace AtomEngine;
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto folder = std::filesystem::path(exe).parent_path() / "Smoke";
    auto find = [](World &w, const std::string &id) {
        for (auto e : w.View<PersistentIdComponent>())
            if (w.GetComponent<PersistentIdComponent>(e).value == id)
                return e;
        return Entity(entt::null);
    };
    auto make = [](World &w, const char *name, BuiltinMesh primitive, Vector3 position,
                   Vector3 scale = Vector3::UNIT_SCALE) {
        auto obj = w.CreateGameObject(name);
        obj->AddComponent<TransformComponent>(position, Quaternion::IDENTITY, scale);
        obj->AddComponent<MeshSourceComponent>(primitive);
        SceneSerializer::RebuildMesh(w, obj->GetHandle());
        return obj;
    };
    World source;
    auto root = make(source, "Prefab root", BuiltinMesh::Cube, Vector3::ZERO);
    source.GetComponent<MaterialComponent>(root->GetHandle()).mMaterials[0].mRoughness = .4f;
    auto prefab = folder / "propagation.prefab";
    SceneSerializer::SavePrefab(source, root->GetHandle(), prefab);
    World instances;
    auto a = SceneSerializer::InstantiatePrefab(instances, prefab),
         b = SceneSerializer::InstantiatePrefab(instances, prefab);
    auto aid = instances.GetComponent<PersistentIdComponent>(a).value,
         bid = instances.GetComponent<PersistentIdComponent>(b).value;
    instances.GetComponent<MaterialComponent>(a).mMaterials[0].mRoughness = .17f;
    source.GetComponent<MaterialComponent>(root->GetHandle()).mMaterials[0].mRoughness = .82f;
    source.GetComponent<TransformComponent>(root->GetHandle()).transition.x = 5;
    auto child = make(source, "Prefab child", BuiltinMesh::Sphere, Vector3(0, 2, 0));
    child->AddComponent<HierarchyComponent>(root->GetComponent<PersistentIdComponent>().value);
    SceneSerializer::SavePrefab(source, root->GetHandle(), prefab);
    SceneSerializer::RefreshPrefabs(instances);
    a = find(instances, aid);
    b = find(instances, bid);
    FeatureCheck(std::abs(instances.GetComponent<MaterialComponent>(a).mMaterials[0].mRoughness - .17f) < 1e-5f &&
                     std::abs(instances.GetComponent<MaterialComponent>(b).mMaterials[0].mRoughness - .82f) < 1e-5f,
                 "Prefab source propagation overwrote a local override");
    auto meshes = instances.View<MeshComponent>();
    FeatureCheck(instances.GetComponent<TransformComponent>(a).transition.x == 5 &&
                     std::distance(meshes.begin(), meshes.end()) == 4,
                 "Prefab transform or new child did not propagate");
    FeatureCheck(SceneSerializer::PrefabOverrideCount(instances, a) > 0, "Prefab override not observable");
    auto differences = SceneSerializer::PrefabOverrides(instances, a);
    FeatureCheck(std::any_of(differences.begin(), differences.end(),
                             [](const auto &s) { return s.find("roughness") != std::string::npos; }),
                 "Prefab diff does not identify the overridden material field");
    auto childSourceId = child->GetComponent<PersistentIdComponent>().value;
    auto childId = instances.GetComponent<PrefabInstanceComponent>(a).members.at(childSourceId);
    instances.DestroyGameObject(find(instances, childId));
    SceneSerializer::RefreshPrefabs(instances);
    FeatureCheck(find(instances, childId) == entt::null, "Prefab refresh lost a deletion override");
    SceneSerializer::RevertPrefab(instances, find(instances, aid));
    FeatureCheck(find(instances, childId) != entt::null, "Prefab revert did not restore a deleted child");
    a = find(instances, aid);
    FeatureCheck(std::abs(instances.GetComponent<MaterialComponent>(a).mMaterials[0].mRoughness - .82f) < 1e-5f,
                 "Prefab revert failed");
    auto local = make(instances, "Local addition", BuiltinMesh::Cube, Vector3(0, 1, 0));
    local->AddComponent<HierarchyComponent>(aid);
    SceneSerializer::RevertPrefab(instances, a);
    FeatureCheck(instances.FindByName("Local addition") == entt::null,
                 "Prefab revert retained an added-object override");
    a = find(instances, aid);
    auto linkedChild = find(instances, childId);
    auto duplicateChild = SceneSerializer::Duplicate(instances, linkedChild);
    FeatureCheck(!instances.HasComponent<PrefabInstanceComponent>(duplicateChild),
                 "Duplicating part of a prefab kept an ambiguous source link");
    instances.DestroyGameObject(duplicateChild);
    instances.GetComponent<MaterialComponent>(a).mMaterials[0].mRoughness = .27f;
    SceneSerializer::ApplyPrefab(instances, a);
    b = find(instances, bid);
    FeatureCheck(std::abs(instances.GetComponent<MaterialComponent>(b).mMaterials[0].mRoughness - .27f) < 1e-5f,
                 "Prefab apply did not update other instances");
    instances.settings.iblIntensity = 2.3f;
    instances.settings.exposure = .6f;
    instances.settings.autoExposure = false;
    instances.settings.hasEditorCamera = true;
    instances.settings.editorPosition = Vector3(3, 4, 5);
    instances.settings.editorFov = 67;
    auto xml = SceneSerializer::ToXml(instances, "Settings");
    World restored;
    SceneSerializer::FromXml(restored, xml);
    FeatureCheck(!restored.settings.autoExposure && restored.settings.exposure == .6f &&
                     restored.settings.iblIntensity == 2.3f && restored.settings.editorFov == 67 &&
                     restored.settings.editorPosition.x == 3,
                 "Scene environment/editor camera roundtrip failed");
    FeatureCheck(SceneSerializer::PrefabOverrideCount(restored, find(restored, aid)) == 0,
                 "Prefab baseline did not persist");
    WriteLog(LogLevel::Info, "Smoke",
             "Prefab propagation, overrides, apply/revert, child additions/deletions and scene settings passed");

    World physics;
    auto floor = make(physics, "Mesh floor", BuiltinMesh::Plane, Vector3::ZERO, Vector3(20, 1, 20));
    floor->AddComponent<ColliderComponent>();
    floor->GetComponent<ColliderComponent>().shape = ColliderShape::Mesh;
    auto falling = make(physics, "Convex body", BuiltinMesh::Cube, Vector3(0, 3, 0));
    falling->AddComponent<ColliderComponent>();
    falling->GetComponent<ColliderComponent>().shape = ColliderShape::ConvexHull;
    falling->AddComponent<RigidBodyComponent>();
    PhysicsWorld sim;
    for (int i = 0; i < 180; ++i)
        sim.Update(physics, 1.f / 60);
    FeatureCheck(falling->GetComponent<TransformComponent>().transition.y > .4f &&
                     falling->GetComponent<TransformComponent>().transition.y < .7f,
                 "Dynamic convex hull fell through triangle mesh");
    FeatureCheck(!CollisionWireEdges(*physics.TryGet<MeshComponent>(falling->GetHandle()), true).empty(),
                 "Convex hull wire geometry failed");
    for (auto type : {JointType::Fixed, JointType::Distance, JointType::Hinge})
    {
        World jointWorld;
        auto anchor = make(jointWorld, "Anchor", BuiltinMesh::Cube, Vector3(0, 4, 0));
        anchor->AddComponent<ColliderComponent>();
        auto body = make(jointWorld, "Body", BuiltinMesh::Cube, Vector3(0, 2, 0));
        body->AddComponent<ColliderComponent>();
        body->AddComponent<RigidBodyComponent>();
        JointComponent joint;
        joint.type = type;
        joint.connectedBody = anchor->GetComponent<PersistentIdComponent>().value;
        joint.connectedAnchor = Vector3(0, -2, 0);
        joint.minDistance = joint.maxDistance = 0;
        body->AddComponent<JointComponent>(joint);
        for (int i = 0; i < 120; ++i)
            sim.Update(jointWorld, 1.f / 60);
        FeatureCheck(std::abs(body->GetComponent<TransformComponent>().transition.y - 2) < .15f,
                     "Joint failed to hold body against gravity");
        auto roundtrip = SceneSerializer::ToXml(jointWorld, "Joint");
        World jointCopy;
        SceneSerializer::FromXml(jointCopy, roundtrip);
        FeatureCheck(jointCopy.GetComponent<JointComponent>(jointCopy.FindByName("Body")).type == type,
                     "Joint serialization failed");
        jointWorld.DestroyGameObject(anchor->GetHandle());
        sim.Update(jointWorld, 1.f / 60);
    }
    WriteLog(LogLevel::Info, "Smoke",
             "Triangle mesh/convex contacts, fixed/distance/hinge constraints and joint serialization passed");

    auto modelPath = folder / "async-model.obj";
    std::ofstream(modelPath) << "o Triangle\nv 0 0 0\nv 1 0 0\nv 0 0 1\nf 1 3 2\n";
    auto async = AssetManager::PreloadAsync(
        {{AssetKind::Texture, "Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KBrdf.dds",
          BuiltinMesh::None, kWhiteOpaque2D, false},
         {AssetKind::Model, modelPath.generic_string()}});
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (!async->Report().Ready() && std::chrono::steady_clock::now() < until)
    {
        async->Step(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FeatureCheck(async->Report().Success(), "Async CPU prepare/GPU commit failed");
    FeatureCheck(AssetManager::LoadModel(modelPath.generic_string())->mGeometry->vertices.size() == 3,
                 "Async model commit lost geometry");
    ScriptAttachment empty;
    FeatureCheck(empty.assembly.empty() && empty.type.empty() && empty.fields.empty(),
                 "Script attachment defaults are hard-coded");
    auto generated = folder / "GeneratedBehaviour.cs";
    std::filesystem::remove(generated);
    ScriptTools::Generate("GeneratedBehaviour", true, folder);
    std::ifstream generatedFile(generated);
    std::string generatedText(std::istreambuf_iterator<char>(generatedFile), {});
    FeatureCheck(generatedText.find("public override void OnUpdate(float dt)") != std::string::npos &&
                     generatedText.find("public override void OnCollision(uint other, bool trigger)") !=
                         std::string::npos &&
                     generatedText.find("Speed") == std::string::npos,
                 "Generated Behaviour callbacks/default fields are incorrect");
    bool refused = false;
    try
    {
        ScriptTools::Generate("GeneratedBehaviour", true, folder);
    }
    catch (const std::exception &)
    {
        refused = true;
    }
    FeatureCheck(refused, "Script generation overwrote an existing file");
    auto v1 = folder / "ReloadV1.dll", v2 = folder / "ReloadV2.dll", active = folder / "ReloadActive.dll";
    FeatureCheck(std::filesystem::exists(v1) && std::filesystem::exists(v2), "Build reload smoke fixtures first");
    std::filesystem::copy_file(v1, active, std::filesystem::copy_options::overwrite_existing);
    World scriptsWorld;
    auto scripted = scriptsWorld.CreateGameObject("Hot reload");
    scripted->AddComponent<TransformComponent>();
    scripted->AddComponent<ScriptComponent>();
    ScriptAttachment script;
    script.assembly = active.generic_string();
    script.type = "Game.ReloadProbe";
    script.fields["Speed"] = "3";
    scripted->GetComponent<ScriptComponent>().scripts.push_back(script);
    auto added = scriptsWorld.CreateGameObject("New type after build");
    added->AddComponent<TransformComponent>();
    added->AddComponent<ScriptComponent>();
    ScriptAttachment addedScript;
    addedScript.assembly = active.generic_string();
    addedScript.type = "Game.AddedProbe";
    added->GetComponent<ScriptComponent>().scripts.push_back(addedScript);
    ScriptSystem scripts;
    scripts.Update(scriptsWorld, .1f);
    FeatureCheck(std::abs(scripted->GetComponent<TransformComponent>().transition.x - .3f) < .001f,
                 "First assembly behaviour failed");
    std::filesystem::copy_file(v2, active, std::filesystem::copy_options::overwrite_existing);
    scripts.Update(scriptsWorld, .5f);
    scripts.Update(scriptsWorld, .5f);
    FeatureCheck(std::abs(scripted->GetComponent<TransformComponent>().transition.x - 4.8f) < .001f,
                 "Automatic stable-file reload failed");
    FeatureCheck(std::abs(added->GetComponent<TransformComponent>().transition.y - .5f) < .001f,
                 "Newly compiled script type did not recover from its initial missing-type error");
    added->GetComponent<ScriptComponent>().scripts.clear();
    scripts.Update(scriptsWorld, 0);
    std::filesystem::copy_file(v1, active, std::filesystem::copy_options::overwrite_existing);
    FeatureCheck(scripts.ReloadAssembly(active), "Collectible assembly reload failed");
    scripts.Update(scriptsWorld, .1f);
    FeatureCheck(std::abs(scripted->GetComponent<TransformComponent>().transition.x - 5.1f) < .001f,
                 "Reloaded code or public field state was not preserved");
    std::ofstream(active, std::ios::binary | std::ios::trunc) << "invalid assembly";
    FeatureCheck(!scripts.ReloadAssembly(active), "Invalid assembly unexpectedly reloaded");
    scripts.Update(scriptsWorld, .1f);
    FeatureCheck(std::abs(scripted->GetComponent<TransformComponent>().transition.x - 5.4f) < .001f,
                 "Failed reload discarded working behaviour");
    scripts.Reset();
    WriteLog(LogLevel::Info, "Smoke",
             "Async resource preparation and C# assembly replacement/state preservation passed");

    ViewRenderResources first, second;
    first.Resize(320, 180);
    second.Resize(192, 128);
    FeatureCheck(first.buffers.shadowBuffer.GetResource() != second.buffers.shadowBuffer.GetResource() &&
                     first.buffers.viewExposure.GetResource() != second.buffers.viewExposure.GetResource(),
                 "View writable resources are shared");
    World emptyView;
    emptyView.settings.skyEnabled = false;
    emptyView.settings.iblIntensity = 0;
    Camera camera;
    camera.SetLookAt(Vector3(0, 3, -6), Vector3::ZERO, Vector3::UP);
    camera.SetAspectRatio(180.f / 320);
    camera.Update();
    auto render = [&](ViewRenderResources &r) {
        auto &color = r.buffers.sceneColorBuffer;
        auto &depth = r.buffers.sceneDepthBuffer;
        D3D12_VIEWPORT vp{0, 0, float(color.GetWidth()), float(color.GetHeight()), 0, 1};
        D3D12_RECT sc{0, 0, LONG(color.GetWidth()), LONG(color.GetHeight())};
        Renderer::Render(RenderView{emptyView, camera, color, depth, vp, sc, &r});
    };
    render(first);
    first.buffers.sceneColorBuffer.ExportToFile((folder / "view-before.raw").wstring());
    render(second);
    first.buffers.sceneColorBuffer.ExportToFile((folder / "view-after.raw").wstring());
    auto bytes = [](const auto &path) {
        std::ifstream f(path, std::ios::binary);
        return std::vector<char>(std::istreambuf_iterator<char>(f), {});
    };
    FeatureCheck(bytes(folder / "view-before.raw") == bytes(folder / "view-after.raw"),
                 "Rendering a second view changed the first view output");
    FeatureCheck(first.buffers.frameIndex == 1 && second.buffers.frameIndex == 1, "Per-view frame history is shared");
    second.Resize(256, 144);
    FeatureCheck(first.buffers.sceneColorBuffer.GetWidth() == 320 && !second.buffers.hasHistory,
                 "Resizing a view affected another view/history");
    RenderGraph invalid;
    invalid.Import(first.buffers.sceneColorBuffer, false);
    invalid.AddPass("Invalid read",
                    {{&first.buffers.sceneColorBuffer, GraphAccess::Read, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE}},
                    [](GraphicsContext &) {});
    bool rejected = false;
    try
    {
        invalid.Compile();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    FeatureCheck(rejected, "Render graph accepted an uninitialized read");
    // Synthetic diagonal: FXAA must introduce partial coverage without altering flat interiors.
    {
        ScopedViewBuffers scope(first.buffers);
        std::vector<uint32_t> diagonal(320 * 180, 3u << 30);
        for (int y = 0; y < 180; ++y)
            for (int x = 0; x < y + 40; ++x)
                diagonal[size_t(y) * 320 + x] = 0xffffffffu;
        Texture pattern;
        pattern.Create2D(320 * 4, 320, 180, DXGI_FORMAT_R10G10B10A2_UNORM, diagonal.data());
        auto &ctx = GraphicsContext::Begin(L"FXAA diagonal regression");
        ctx.CopyBuffer(gSceneColorBuffer, pattern);
        FXAA::Render(ctx);
        ctx.Finish(true);
        gFxaaBuffer.ExportToFile((folder / "fxaa-diagonal.raw").wstring());
    }
    auto data = bytes(folder / "fxaa-diagonal.raw");
    uint32_t header[4]{};
    std::memcpy(header, data.data(), 16);
    size_t smooth = 0;
    for (size_t offset = 16; offset + 4 <= data.size(); offset += 4)
    {
        uint32_t p;
        std::memcpy(&p, data.data() + offset, 4);
        auto value = p & 1023;
        if (value > 10 && value < 1013)
            ++smooth;
    }
    FeatureCheck(smooth > 100, "FXAA did not smooth a diagonal edge");
    auto pixel = [&](size_t x, size_t y) {
        uint32_t value;
        std::memcpy(&value, data.data() + 16 + y * header[1] + x * 4, 4);
        return value & 1023;
    };
    FeatureCheck(pixel(8, 8) == 1023 && pixel(300, 8) == 0, "FXAA altered a flat interior");
    auto originalWidth = gSceneColorBuffer.GetWidth(), originalHeight = gSceneColorBuffer.GetHeight();
    DX12Core::gCommandManager.IdleGPU();
    InitializeBuffers(384, 216);
    Renderer::Update(emptyView, 0);
    Renderer::Render(emptyView, camera, 0);
    FeatureCheck(GetPrimaryViewBuffers().frameIndex == 1, "Primary resize did not reset its frame history");
    DX12Core::gCommandManager.IdleGPU();
    InitializeBuffers(originalWidth, originalHeight);
    Renderer::Update(emptyView, 0);
    WriteLog(LogLevel::Info, "Smoke",
             "FXAA edge pixels, render graph validation, two-view output/history and independent resize passed");
}
