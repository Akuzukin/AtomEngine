#pragma once
#include "Editor/EditorApp.h"
#include "Runtime/Function/Scene/GameProject.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "imgui.h"
#include "json.hpp"

class ProjectWorkflowSmokeApp final : public AtomEngine::EditorApp
{
    int frame = 0, switches = 0;
    std::string lastScene, beforePlay;
    std::filesystem::path directory;
    std::weak_ptr<void> previousWorld;
    bool expectedLoadFailure = false, loadFailureSeen = false;
    std::string secondSceneXml;
    std::weak_ptr<void> failedLoadWorld;
public:
    void Initialize() override
    {
        using namespace AtomEngine;
        EditorApp::Initialize(); ImGui::GetIO().IniFilename = nullptr;
        auto originalDirectory = std::filesystem::current_path();
        ProjectPanel panel;
        auto created = EngineDirectory() / "Smoke/NewProject/Fresh.atproj";
        panel.Activate(mEditor, *mSceneManager.GetCurrentScene(), created, true);
        FeatureCheck(std::filesystem::is_regular_file(created) && std::filesystem::is_regular_file(created.parent_path() / "Managed/GameScripts/GameScripts.csproj"), "New project scaffold failed");
        panel.Activate(mEditor, *mSceneManager.GetCurrentScene(), created, false);
        FeatureCheck(ActiveProject.project->scenes.size() == 1 && ActiveProject.currentScene == "Asset/Scenes/Start.scene", "Open project failed");
        std::filesystem::current_path(originalDirectory);
        directory = EngineDirectory() / "Smoke/ProjectWorkflow";
        std::filesystem::create_directories(directory / "Asset/Scenes");
        GameProject project; project.file = directory / "Regression.atproj";
        project.settings.startupScene = "Asset/Scenes/First.scene";
        project.settings.name = "多场景项目";
        project.scriptProject.clear(); project.scenes = {"Asset/Scenes/First.scene", "Asset/Scenes/Second.scene"};
        for (int i = 0; i < 2; ++i)
        {
            World world; auto object = world.CreateGameObject("Scene probe"); object->AddComponent<TransformComponent>();
            ScriptAttachment attachment; attachment.assembly = "Scripts/AtomScripts.dll"; attachment.type = "Game.ProjectSceneProbe";
            attachment.fields["Destination"] = project.scenes[1-i];
            object->AddComponent<ScriptComponent>(); object->GetComponent<ScriptComponent>().scripts.push_back(attachment);
            SceneSerializer::Save(world, directory / UTF8ToWString(project.scenes[i]), i ? "Second" : "First");
        }
        project.Save(); auto loaded = GameProject::Load(project.file);
        { std::ifstream file(directory / UTF8ToWString(project.scenes[1])); secondSceneXml.assign(std::istreambuf_iterator<char>(file), {}); }
        FeatureCheck(loaded.settings.name == project.settings.name && loaded.scenes == project.scenes, "Project round trip failed");
        FeatureCheck(loaded.FindScene("Second") == project.scenes[1], "Scene name lookup failed");
        bool rejected = false; try { GameProject::RelativePath("../escape.scene"); } catch (...) { rejected = true; }
        FeatureCheck(rejected, "Project path traversal accepted");
        auto invalid = loaded; invalid.scenes.push_back(invalid.scenes.front()); rejected = false;
        try { invalid.Save(); } catch (...) { rejected = true; }
        FeatureCheck(rejected && GameProject::Load(project.file).scenes.size() == 2, "Invalid save overwrote valid project");
        ActiveProject.project = loaded; ActiveProject.currentScene = project.scenes.front();
        mSceneManager.GetCurrentScene()->Load(directory / UTF8ToWString(project.scenes.front()));
        mEditor.Initialize(directory / UTF8ToWString(project.scenes.front()));
        mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        beforePlay = SceneSerializer::ToXml(GetWorld(), "First"); lastScene = ActiveProject.currentScene; previousWorld = GetWorld().GetLifetime();
    }
    void OnRuntimeError(const std::string& error) override { FeatureCheck(error.empty(), error.c_str()); }
    void OnSceneLoadError(const std::string& error) override { FeatureCheck(expectedLoadFailure && !error.empty(), "Unexpected scene load failure"); loadFailureSeen = true; }
    void Update(float dt) override
    {
        using namespace AtomEngine;
        if (ActiveProject.currentScene != lastScene)
        {
            FeatureCheck(previousWorld.expired(), "Old scene lifetime survived replacement");
            previousWorld = GetWorld().GetLifetime(); lastScene = ActiveProject.currentScene; ++switches;
        }
        if (++frame == 35)
        {
            FeatureCheck(switches >= 5, "Managed multi-scene switching did not repeat");
            auto pending = ActiveProject.pendingScene; bool rejected = false;
            try { ActiveProject.RequestScene("NotInProject"); } catch (...) { rejected = true; }
            FeatureCheck(rejected && ActiveProject.pendingScene == pending, "Invalid scene request changed pending transition");
            mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
            FeatureCheck(SceneSerializer::ToXml(GetWorld(), "First") == beforePlay, "Stop did not restore the original edit scene");
            FeatureCheck(!ActiveProject.pendingScene, "Stop left a pending scene transition");
        }
        if (frame == 42) { mEditor.TogglePlay(*mSceneManager.GetCurrentScene()); lastScene = ActiveProject.currentScene; previousWorld = GetWorld().GetLifetime(); }
        if (frame == 45)
        {
            expectedLoadFailure = true; failedLoadWorld = GetWorld().GetLifetime();
            std::ofstream(directory / "Asset/Scenes/Second.scene") << "<Scene version=\"999\"/>";
        }
        if (frame == 46)
        {
            FeatureCheck(loadFailureSeen && !failedLoadWorld.expired() && ActiveProject.currentScene == "Asset/Scenes/First.scene" && !ActiveProject.lastLoadError.empty(), "Failed switch lost the old world/state");
            auto fields = nlohmann::json::parse(gContext.readScriptFields(GetWorld(), GetWorld().FindByName("Scene probe"), 0));
            FeatureCheck(fields.at("Updates").get<std::string>() == "4", "Failed switch recreated the running script");
            std::ofstream(directory / "Asset/Scenes/Second.scene") << secondSceneXml;
            expectedLoadFailure = false; ActiveProject.RequestScene("Second");
        }
        if (frame == 47) FeatureCheck(ActiveProject.lastLoadError.empty(), "Successful switch retained stale errors");
        if (frame == 70) { FeatureCheck(switches >= 10, "Repeated Play failed to switch scenes"); mEditor.TogglePlay(*mSceneManager.GetCurrentScene()); }
        EditorApp::Update(dt);
        if (frame == 72) WriteLog(LogLevel::Info, "Project", "Project persistence, managed scene switching, repeated Play and Stop restoration passed");
    }
    void Shutdown() override { EditorApp::Shutdown(); AtomEngine::ActiveProject = {}; }
};
