#include "../EditorPanelSupport.h"
#include "../ProjectBuildTools.h"
#include "Runtime/Function/Scene/GameProject.h"
namespace AtomEngine
{
std::filesystem::path ProjectPanel::Dialog(bool save)
{
    wchar_t path[32768]{}; OPENFILENAMEW dialog{}; dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gContext.windowManager->GetWindowHandle(); dialog.lpstrFile = path; dialog.nMaxFile = DWORD(std::size(path));
    dialog.lpstrFilter = L"Atom project (*.atproj)\0*.atproj\0\0"; dialog.lpstrDefExt = L"atproj";
    dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) ? std::filesystem::path(path) : std::filesystem::path{};
}
void ProjectPanel::Activate(EditorLayer& editor, Scene& scene, const std::filesystem::path& path, bool create)
{
    auto previousDirectory = std::filesystem::current_path(); auto previousDefaults = ProjectSceneDefaults;
    GameProject project;
    if (create)
    {
        project.file = std::filesystem::absolute(path); project.settings.name = WStringToUTF8(path.stem().wstring());
        project.settings.startupScene = "Asset/Scenes/Start.scene"; project.scenes = {project.settings.startupScene}; project.scriptProject.clear();
        std::filesystem::create_directories(project.Root() / "Asset/Scenes");
        for (auto dir : {"Prefabs", "Textures", "Sounds", "Materials", "Shaders"}) std::filesystem::create_directories(project.Root() / "Asset" / dir);
        auto initial = project.Root() / "Asset/Scenes/Start.scene";
        if (!std::filesystem::exists(initial)) { World empty; empty.settings = project.settings.defaults; SceneSerializer::Save(empty, initial, "Start"); }
        auto engine = EngineSourceDirectory(); auto scripts = project.Root() / "Managed/GameScripts";
        std::filesystem::create_directories(scripts);
        auto csproj = scripts / "GameScripts.csproj";
        if (!std::filesystem::exists(csproj))
        {
            tinyxml2::XMLDocument doc; auto root = doc.NewElement("Project"); root->SetAttribute("Sdk", "Microsoft.NET.Sdk"); doc.InsertEndChild(root);
            auto props = doc.NewElement("PropertyGroup"); root->InsertEndChild(props);
            for (auto pair : {std::pair{"TargetFramework", "net10.0"}, {"AssemblyName", "AtomScripts"}, {"ImplicitUsings", "enable"}, {"Nullable", "enable"}})
            { auto n = doc.NewElement(pair.first); n->SetText(pair.second); props->InsertEndChild(n); }
            auto items = doc.NewElement("ItemGroup"); root->InsertEndChild(items); auto reference = doc.NewElement("ProjectReference");
            auto sdk = doc.NewElement("AtomEngineSdkProject"); sdk->SetAttribute("Condition", "'$(AtomEngineSdkProject)'==''"); sdk->SetText(WStringToUTF8((engine / "Managed/Atom.Managed/Atom.Managed.csproj").wstring()).c_str()); props->InsertEndChild(sdk);
            reference->SetAttribute("Include", "$(AtomEngineSdkProject)"); items->InsertEndChild(reference);
            tinyxml2::XMLPrinter printer; doc.Print(&printer);
            std::ofstream stream(csproj, std::ios::binary); stream << printer.CStr(); stream.close();
            if (!stream) throw std::runtime_error("Cannot create script project");
        }
        project.scriptProject = "Managed/GameScripts/GameScripts.csproj"; project.Save();
        auto workspace = project.Root() / "Managed/AtomScripts.code-workspace";
        if (!std::filesystem::exists(workspace)) { std::ofstream stream(workspace); stream << "{\"folders\":[{\"path\":\".\"}]}"; }
    }
    else project = GameProject::Load(path);
    try
    {
        std::filesystem::current_path(project.Root()); ProjectSceneDefaults = project.settings.defaults;
        scene.Load(UTF8ToWString(project.settings.startupScene));
    }
    catch (...) { std::filesystem::current_path(previousDirectory); ProjectSceneDefaults = previousDefaults; throw; }
    ActiveProject = {}; ActiveProject.project = project; ActiveProject.currentScene = project.settings.startupScene;
    editor.ClearThumbnails(); editor.mAssetPreview = nullptr; editor.mAssetSelection.clear(); editor.mScriptSchemas.clear(); editor.mPrefabStamps.clear();
    editor.mMaterialPreview.reset(); editor.mPreset = MaterialPreset{}; strncpy_s(editor.mMaterialPath, "Asset/Materials/New.mat", _TRUNCATE); editor.mAssetEntries.clear();
    editor.mScenePreload.reset(); editor.mAssetDirectory = "Asset"; editor.mRefreshAssets = true;
    editor.Select(scene.GetWorld(), entt::null); editor.mDocument.Reset(scene.GetWorld(), scene.GetName()); editor.mDocumentReady = true;
    strncpy_s(editor.mScenePath, project.settings.startupScene.c_str(), _TRUNCATE);
    editor.mProjectSettingsPanel.Reload(); editor.SetMaximizeOnPlay(project.settings.maximizeOnPlay);
    SetWindowTextW(gContext.windowManager->GetWindowHandle(), UTF8ToWString(project.settings.name + " - Atom Editor").c_str());
    visible = true; message.clear();
}
bool ProjectPanel::Save(EditorLayer& editor, Scene& scene)
{
    if (!ActiveProject.project) { message = "Create or open an .atproj project first"; visible = true; return false; }
    editor.Capture(scene);
    if (editor.mDocument.Dirty() && !editor.SaveDocument(scene)) return false;
    bool saved = false;
    editor.Run([&] { ActiveProject.project->Save(); ActiveProject.dirty = false; message = "Project saved"; saved = true; }); return saved;
}
void ProjectPanel::StartBuild(EditorLayer& editor, Scene& scene, bool publish)
{
    visible = true;
    if (Busy() || !Save(editor, scene)) return;
    editor.Run([&] {
        auto output = std::filesystem::path(UTF8ToWString(ActiveProject.project->buildOutput));
        if (output.empty()) throw std::runtime_error("Build output is empty");
        if (output.is_relative()) output = ActiveProject.project->Root() / output;
        build = BuildProject(ActiveProject.project->file, output, publish); message = "Building game...";
    });
}
void ProjectPanel::Draw(EditorLayer& editor, Scene& scene)
{
    if (build.valid() && build.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try { auto result = build.get(); message = result ? "Build failed; see logs/ProjectBuild.log" : "Game build completed"; WriteLog(result ? LogLevel::Error : LogLevel::Info, "Build", message); }
        catch (const std::exception& error) { message = error.what(); WriteLog(LogLevel::Error, "Build", message); }
    }
    if (!visible) return;
    ImGui::SetNextWindowSize(ImVec2(650, 540), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(EditorLocalization::Label("Project"), &visible)) { ImGui::End(); return; }
    ImGui::BeginDisabled(Busy() || editor.IsPlaying());
    if (!ActiveProject.project) ImGui::TextWrapped("%s", EditorLocalization::Text("Create or open an .atproj project first"));
    else
    {
        auto& project = *ActiveProject.project;
        ImGui::TextWrapped("%s%s", PathText(project.file).c_str(), ActiveProject.dirty ? " *" : "");
        ActiveProject.dirty |= Text("Project name", project.settings.name);
        ActiveProject.dirty |= Text("Script project", project.scriptProject);
        ActiveProject.dirty |= ImGui::InputInt(EditorLocalization::Label("Window width"), &project.width);
        ActiveProject.dirty |= ImGui::InputInt(EditorLocalization::Label("Window height"), &project.height);
        ActiveProject.dirty |= ImGui::Checkbox(EditorLocalization::Label("Fullscreen"), &project.fullscreen);
        ImGui::SeparatorText(EditorLocalization::Text("Project scenes"));
        for (size_t i = 0; i < project.scenes.size(); ++i)
        {
            ImGui::PushID(int(i)); auto path = project.scenes[i];
            if (ImGui::RadioButton("##startup", path == project.settings.startupScene)) { project.settings.startupScene = path; ActiveProject.dirty = true; }
            EditorTheme::Tooltip(EditorLocalization::Text("Startup scene")); ImGui::SameLine();
            if (ImGui::Selectable(path.c_str(), ActiveProject.currentScene == path, 0, ImVec2(ImGui::GetContentRegionAvail().x - 65, 0))) editor.OpenScene(project.Root() / UTF8ToWString(path));
            ImGui::SameLine();
            if (ImGui::SmallButton(EditorLocalization::Label("Remove")))
            { project.scenes.erase(project.scenes.begin() + i); if (project.settings.startupScene == path) project.settings.startupScene = project.scenes.empty() ? "" : project.scenes.front(); ActiveProject.dirty = true; ImGui::PopID(); break; }
            ImGui::PopID();
        }
        ImGui::InputText(EditorLocalization::Label("Scene path"), scenePath, sizeof(scenePath));
        if (ImGui::Button(EditorLocalization::Label("Add scene"))) editor.Run([&] { auto path = GameProject::RelativePath(UTF8ToWString(scenePath)); if (!std::filesystem::is_regular_file(project.Root() / UTF8ToWString(path))) throw std::runtime_error("Scene does not exist"); if (std::find(project.scenes.begin(), project.scenes.end(), path) == project.scenes.end()) { project.scenes.push_back(path); if (project.settings.startupScene.empty()) project.settings.startupScene = path; ActiveProject.dirty = true; } });
        ImGui::SameLine(); if (ImGui::Button(EditorLocalization::Label("Add current scene"))) editor.Run([&] {
            if (!editor.SaveDocument(scene)) return;
            auto path = GameProject::RelativePath(std::filesystem::absolute(UTF8ToWString(editor.mScenePath)).lexically_relative(project.Root()));
            if (std::find(project.scenes.begin(), project.scenes.end(), path) == project.scenes.end()) { project.scenes.push_back(path); if (project.settings.startupScene.empty()) project.settings.startupScene = path; ActiveProject.dirty = true; }
        });
        ImGui::SeparatorText(EditorLocalization::Text("Additional game assets"));
        ImGui::TextWrapped("%s", EditorLocalization::Text("Add files or folders loaded dynamically by scripts"));
        for (size_t i = 0; i < project.additionalAssets.size(); ++i) { ImGui::PushID(int(i) + 10000); ImGui::TextUnformatted(project.additionalAssets[i].c_str()); ImGui::SameLine(); if (ImGui::SmallButton(EditorLocalization::Label("Remove"))) { project.additionalAssets.erase(project.additionalAssets.begin() + i); ActiveProject.dirty = true; ImGui::PopID(); break; } ImGui::PopID(); }
        ImGui::InputText(EditorLocalization::Label("Asset path"), includePath, sizeof(includePath));
        if (ImGui::Button(EditorLocalization::Label("Include asset"))) editor.Run([&] { auto path = GameProject::RelativePath(UTF8ToWString(includePath)); if (!std::filesystem::exists(project.Root() / UTF8ToWString(path))) throw std::runtime_error("Asset does not exist"); project.additionalAssets.push_back(path); ActiveProject.dirty = true; });
        ActiveProject.dirty |= Text("Build output", project.buildOutput);
        if (ImGui::Button(EditorLocalization::Label("Save project"))) Save(editor, scene);
        ImGui::SameLine(); if (ImGui::Button(EditorLocalization::Label("Build game"))) StartBuild(editor, scene, false);
        ImGui::SameLine(); if (ImGui::Button(EditorLocalization::Label("Build and publish"))) StartBuild(editor, scene, true);
    }
    ImGui::EndDisabled();
    if (Busy()) ImGui::TextUnformatted(EditorLocalization::Text("Building game..."));
    if (!message.empty()) ImGui::TextWrapped("%s", EditorLocalization::Text(message.c_str()));
    ImGui::End();
}
}
