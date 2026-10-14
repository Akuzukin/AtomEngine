#pragma once
#include "Editor/EditorDocument.h"
#include "Editor/EditorLayer.h"
#include "Runtime/Function/Scene/SceneManager.h"
#include "Runtime/Function/Scene/SceneRuntime.h"
inline void RunEditorSmokeChecks()
{
    using namespace AtomEngine;
    {
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,DWORD(std::size(executable)));
        auto file=std::filesystem::path(executable).parent_path()/"Smoke/ProjectSettings.xml";
        ProjectSettings project;project.name="Panel project test";project.startupScene="Asset/Scenes/MaterialTest.scene";project.defaults.skyIntensity=2;project.defaults.exposure=.6f;project.defaults.fxaa=false;project.maximizeOnPlay=true;project.Save(file);
        auto loaded=ProjectSettings::Load(file);FeatureCheck(loaded.name==project.name && loaded.startupScene==project.startupScene && loaded.defaults.skyIntensity==2 && loaded.defaults.exposure==.6f && !loaded.defaults.fxaa && loaded.maximizeOnPlay,"Project settings save/load failed");
        auto previous=ProjectSceneDefaults;ProjectSceneDefaults=loaded.defaults;World fresh;FeatureCheck(fresh.settings.skyIntensity==2 && !fresh.settings.fxaa,"New world ignored project defaults");
        World saved;saved.settings.skyIntensity=3;auto xml=SceneSerializer::ToXml(saved,"Override");World restored;SceneSerializer::FromXml(restored,xml);FeatureCheck(restored.settings.skyIntensity==3,"Project defaults overwrote saved scene");ProjectSceneDefaults=previous;
        bool rejected=false;loaded.defaults.skySunSize=-1;try{loaded.Save(file);}catch(...){rejected=true;}FeatureCheck(rejected && ProjectSettings::Load(file).defaults.skySunSize==project.defaults.skySunSize,"Invalid project settings damaged saved defaults");
        WriteLog(LogLevel::Info,"Smoke","Project settings roundtrip, new-world defaults, scene overrides and invalid-save preservation passed");
    }
    World world;
    auto parentObject = world.CreateGameObject("Parent");
    auto childObject = world.CreateGameObject("Child");
    Entity parent = *parentObject, child = *childObject;
    world.AddComponent<TransformComponent>(parent);
    world.AddComponent<TransformComponent>(child);
    world.GetComponent<TransformComponent>(parent).transition = Vector3(5, 2, 3);
    world.GetComponent<TransformComponent>(parent).scale = Vector3(2, 2, 2);
    world.GetComponent<TransformComponent>(child).transition = Vector3(1, 3, 4);
    auto sameMatrix = [](const Matrix4x4 &a, const Matrix4x4 &b) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (std::abs(a[i][j] - b[i][j]) > .002f)
                    return false;
        return true;
    };
    auto original = world.GetComponent<TransformComponent>(child).GetMatrix();
    EditorActions::Reparent(world, child, parent);
    FeatureCheck(sameMatrix(original, world.GetComponent<TransformComponent>(child).GetMatrix()),
                 "Reparent changed world transform");
    auto beforeCycle = SceneSerializer::ToXml(world, "Test");
    bool rejected = false;
    try
    {
        EditorActions::Reparent(world, parent, child);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    FeatureCheck(rejected && SceneSerializer::ToXml(world, "Test") == beforeCycle, "Cycle rejection changed scene");
    auto parentId = EditorActions::Id(world, parent), childId = EditorActions::Id(world, child);
    EditorDocument history;
    history.Reset(world, "Test");
    world.GetComponent<TransformComponent>(child).transition.x += 3;
    FeatureCheck(history.Capture(world, "Test", childId, "Move") && history.Dirty(), "Edit was not recorded");
    history.MarkSaved(world, "Test");
    world.GetComponent<TransformComponent>(child).transition.y += 2;
    history.Capture(world, "Test", childId, "Move");
    history.Undo(world);
    FeatureCheck(!history.Dirty() && history.CanRedo(), "Undo did not restore save point");
    FeatureCheck(!history.Capture(world, "Test", childId), "Undo introduced a phantom edit");
    history.Redo(world);
    FeatureCheck(history.Dirty(), "Redo did not restore dirty state");
    history.Undo(world);
    child = EditorActions::Find(world, childId);
    parent = EditorActions::Find(world, parentId);
    world.GetComponent<TransformComponent>(child).transition.z += 1;
    history.Capture(world, "Test", childId);
    FeatureCheck(!history.CanRedo(), "New edit retained redo branch");
    world.AddComponent<ScriptComponent>(child);
    world.GetComponent<ScriptComponent>(child).scripts.push_back({});
    world.GetComponent<ScriptComponent>(child).scripts[0].fields["speed"] = "2";
    auto copy = SceneSerializer::Duplicate(world, parent);
    Entity copyChild = entt::null;
    for (auto e : world.View<HierarchyComponent>())
        if (world.GetComponent<HierarchyComponent>(e).parentId == EditorActions::Id(world, copy))
            copyChild = e;
    FeatureCheck(world.IsValid(copyChild) && EditorActions::Id(world, copyChild) != childId,
                 "Duplicate did not remap subtree IDs");
    world.GetComponent<ScriptComponent>(copyChild).scripts[0].fields["speed"] = "9";
    FeatureCheck(world.GetComponent<ScriptComponent>(child).scripts[0].fields["speed"] == "2",
                 "Duplicate shared script properties");
    EditorActions::DeleteTree(world, copy);
    FeatureCheck(!world.IsValid(copyChild) && world.IsValid(child), "Delete tree removed wrong objects");
    auto matrix = world.GetComponent<TransformComponent>(child).GetMatrix();
    EditorActions::Reparent(world, child, entt::null);
    FeatureCheck(sameMatrix(matrix, world.GetComponent<TransformComponent>(child).GetMatrix()),
                 "Detach changed world transform");
    EditorActions::SetWorldMatrix(world, child, original);
    FeatureCheck(sameMatrix(original, world.GetComponent<TransformComponent>(child).GetMatrix()),
                 "Gizmo transform conversion failed");
    auto shear = original;
    shear[0][1] = .4f;
    auto valid = SceneSerializer::ToXml(world, "Test");
    rejected = false;
    try
    {
        EditorActions::SetWorldMatrix(world, child, shear);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    FeatureCheck(rejected && SceneSerializer::ToXml(world, "Test") == valid, "Unsupported transform changed scene");
    Camera camera;
    auto center = EditorActions::FrameSelection(world, child, camera);
    FeatureCheck(std::isfinite(center.x + center.y + center.z), "Frame selection produced invalid camera");
    auto xml = SceneSerializer::ToXml(world, "Test");
    World restored;
    FeatureCheck(SceneSerializer::FromXml(restored, xml) == "Test" && SceneSerializer::ToXml(restored, "Test") == xml,
                 "Scene snapshot round trip changed data");
    class TestScene final : public Scene
    {
        Camera camera;

      public:
        TestScene(SceneManager &manager) : Scene("Test", manager)
        {
            mCamera = &camera;
        }
        void Update(float) override
        {
        }
    };
    SceneManager manager;
    TestScene scene(manager);
    SceneSerializer::FromXml(scene.GetWorld(), xml);
    EditorLayer editor;
    editor.Select(scene.GetWorld(), EditorActions::Find(scene.GetWorld(), childId));
    auto editPosition = scene.GetCamera().GetPosition();
    editor.SetMaximizeOnPlay(true);
    editor.TogglePlay(scene);
#ifndef RELEASE
    FeatureCheck(editor.IsPlayMaximized(), "Play maximize option did not start");
#endif
    editor.TogglePause();
    FeatureCheck(editor.IsPaused(), "Pause did not suspend play");
    editor.StepFrame();
    FeatureCheck(!editor.IsPaused() && std::abs(editor.RuntimeDeltaTime(1) - 1.f / 60) < 1e-5f,
                 "Single frame timing failed");
    editor.TogglePause();
    FeatureCheck(!editor.IsPaused(), "Resume failed");
    scene.GetCamera().SetPosition(Vector3(99, 99, 99));
    FeatureCheck(editor.IsPlaying(), "Play did not start");
    scene.GetWorld().GetComponent<TransformComponent>(editor.Selected()).transition.x += 10;
    scene.SetName("Runtime name");
    editor.TogglePlay(scene);
    FeatureCheck((scene.GetCamera().GetPosition() - editPosition).Length() < .001f && !editor.IsPlayMaximized(),
                 "Stop did not restore editor camera/maximize state");
    FeatureCheck(!editor.IsPlaying() && scene.GetName() == "Test" &&
                     SceneSerializer::ToXml(scene.GetWorld(), scene.GetName()) == xml &&
                     EditorActions::Id(scene.GetWorld(), editor.Selected()) == childId,
                 "Stop did not restore edit scene and selection");
    editor.Select(scene.GetWorld(), entt::null);
    world.RemoveComponent<ScriptComponent>(child);
    world.AddComponent<ColliderComponent>(child);
    world.AddComponent<RigidBodyComponent>(child);
    SceneRuntime runtime;
    runtime.Update(world, 1.f / 60, true, false);
    auto paused = SceneSerializer::ToXml(world, "Test");
    runtime.Update(world, 1.f, true, false, nullptr, true);
    FeatureCheck(SceneSerializer::ToXml(world, "Test") == paused, "Paused runtime changed scene");
    runtime.Update(world, 1.f / 60, true, false);
    FeatureCheck(SceneSerializer::ToXml(world, "Test") != paused, "Resume did not advance physics");
    runtime.Reset();
    for (int i = 0; i < 110; ++i)
    {
        world.GetComponent<TransformComponent>(child).transition.x += 1;
        history.Capture(world, "Test", childId);
    }
    FeatureCheck(history.Size() <= 100, "Undo history exceeded its limit");
    WriteLog(LogLevel::Info, "Smoke",
             "Editor checks passed: history, hierarchy, duplication, gizmo transform, snapshots and pause");
}
