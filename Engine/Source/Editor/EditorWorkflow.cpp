#include "EditorLayer.h"
#include "ScriptTools.h"
#include "ImGuiCommon.h"
#include "EditorTheme.h"
#include "EditorLocalization.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "External/tinyxml2/tinyxml2.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/SelectionRenderer.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "imgui_internal.h"
#include <algorithm>
#include <set>
#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
namespace AtomEngine
{
void EditorLayer::ScriptCreation(World &world)
{
    ImGui::BeginDisabled(mPlaying || mScriptBuild.valid());
    if (ImGui::Button(EditorLocalization::Label("Create C# script")))
        ImGui::OpenPopup(EditorLocalization::Label("New Behaviour"));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mScriptBuild.valid());
    if (ImGui::Button(EditorLocalization::Label("Build scripts")))
        Run([&] { mScriptBuild = ScriptTools::Build(); });
    ImGui::EndDisabled();
    if (ImGui::BeginPopup(EditorLocalization::Label("New Behaviour")))
    {
        static char name[128] = "";
        static bool callbacks = true, attach = true;
        ImGui::InputText(EditorLocalization::Label("Class name"), name, sizeof(name));
        ImGui::Checkbox(EditorLocalization::Label("Generate Behaviour overrides"), &callbacks);
        ImGui::BeginDisabled(!world.IsValid(mSelected));
        ImGui::Checkbox(EditorLocalization::Label("Attach to selected object"), &attach);
        ImGui::EndDisabled();
        if (ImGui::Button(EditorLocalization::Label("Generate")))
            Run([&] {
                auto file = ScriptTools::Generate(name, callbacks);
                mAssetDirectory = file.parent_path();
                if (attach && world.IsValid(mSelected))
                {
                    if (!world.HasComponent<ScriptComponent>(mSelected))
                        world.AddComponent<ScriptComponent>(mSelected);
                    ScriptAttachment script;
                    script.assembly = "Scripts/AtomScripts.dll";
                    script.type = std::string("Game.") + name;
                    auto &scripts = world.GetComponent<ScriptComponent>(mSelected).scripts;
                    if (!scripts.empty() && scripts.back().assembly.empty() && scripts.back().type.empty())
                        scripts.back() = script;
                    else
                        scripts.push_back(script);
                    mHistoryPending = true;
                }
                mRefreshAssets = true;
                ImGui::CloseCurrentPopup();
            });
        ImGui::EndPopup();
    }
}
void EditorLayer::PollBackgroundWork(Scene &scene)
{
    mPrefabWatchTimer += ImGui::GetIO().DeltaTime;
    if (!mPlaying && !mGizmoActive && !ImGui::GetIO().WantTextInput && mPrefabWatchTimer >= 1)
    {
        mPrefabWatchTimer = 0;
        std::set<std::filesystem::path> sources;
        for (auto e : scene.GetWorld().View<PrefabInstanceComponent>())
        {
            const auto &p = scene.GetWorld().GetComponent<PrefabInstanceComponent>(e);
            if (!p.source.empty() && !p.baseline.empty())
                sources.insert(std::filesystem::absolute(p.source).lexically_normal());
        }
        for (const auto &source : sources)
        {
            std::error_code ec;
            auto stamp = std::filesystem::last_write_time(source, ec);
            if (ec)
                continue;
            auto [it, inserted] = mPrefabStamps.emplace(source, stamp);
            if (!inserted && it->second != stamp)
            {
                it->second = stamp;
                Run([&] {
                    auto id = EditorActions::Id(scene.GetWorld(), mSelected);
                    SceneSerializer::RefreshPrefabs(scene.GetWorld(), source);
                    Select(scene.GetWorld(), EditorActions::Find(scene.GetWorld(), id));
                    mHistoryPending = true;
                });
            }
        }
    }
    if (mScriptBuild.valid() && mScriptBuild.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        int code = mScriptBuild.get();
        WriteLog(code ? LogLevel::Error : LogLevel::Info, "Scripts",
                 code ? "Script build failed; see Scripts/ScriptBuild.log. Running assembly retained."
                      : "Scripts built. Active assemblies reload automatically while playing.");
    }
    if (!mScenePreload)
        return;
    auto report = mScenePreload->Step(1);
    ImGui::Begin(EditorLocalization::Label("Loading scene"));
    ImGui::Text(EditorLocalization::Text("Assets: %zu / %zu"), report.completed, report.total);
    ImGui::ProgressBar(report.total ? float(report.completed) / report.total : 1);
    ImGui::End();
    if (!report.Ready())
        return;
    if (!report.Success())
    {
        mError = report.errors.front();
        mScenePreload.reset();
        return;
    }
    auto preload = std::move(mScenePreload);
    if (SceneSerializer::ToXml(scene.GetWorld(), scene.GetName()) != mPreloadSnapshot)
    {
        mError = "Scene changed while loading. Save your changes and open the scene again; resources are now cached.";
        return;
    }
    Run([&] {
        scene.Load(mPreloadPath);
        Select(scene.GetWorld(), entt::null);
        strncpy_s(mScenePath, WStringToUTF8(mPreloadPath.wstring()).c_str(), _TRUNCATE);
        Recent(mPreloadPath);
        mDocument.Reset(scene.GetWorld(), scene.GetName());
        mDocumentReady = true;
    });
}
namespace
{
std::filesystem::path Preferences()
{
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
    return std::filesystem::path(exe).parent_path() / "EditorPreferences.xml";
}
std::filesystem::path SceneDialog(bool save, const char *initial)
{
    wchar_t file[32768]{};
    auto wide = UTF8ToWString(initial);
    if (save)
        wcsncpy_s(file, wide.c_str(), _TRUNCATE);
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = gContext.windowManager->GetWindowHandle();
    dialog.lpstrFilter = L"AtomEngine scene (*.scene)\0*.scene\0\0";
    dialog.lpstrFile = file;
    dialog.nMaxFile = DWORD(std::size(file));
    dialog.lpstrDefExt = L"scene";
    auto title = UTF8ToWString(EditorLocalization::Text(save ? "Save scene" : "Open scene"));
    dialog.lpstrTitle = title.c_str();
    dialog.Flags =
        OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog))
        return file;
    if (auto error = CommDlgExtendedError())
        WriteLog(LogLevel::Error, "Editor", "File dialog failed: " + std::to_string(error));
    return {};
}
std::string PathText(const std::filesystem::path &p)
{
    return WStringToUTF8(p.generic_wstring());
}
std::string Extension(const std::filesystem::path &p)
{
    auto s = p.extension().string();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
bool TextureFile(const std::filesystem::path &p)
{
    auto s = Extension(p);
    return s == ".dds" || s == ".png" || s == ".jpg" || s == ".jpeg" || s == ".bmp" || s == ".tga" || s == ".hdr" ||
           s == ".exr";
}
bool ModelFile(const std::filesystem::path &p)
{
    auto s = Extension(p);
    return s == ".obj" || s == ".fbx" || s == ".gltf" || s == ".glb" || s == ".dae" || s == ".ply" || s == ".stl";
}
void DockLayout(ImGuiID id)
{
    auto viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, viewport->WorkSize);
    auto middle = id;
    auto left = ImGui::DockBuilderSplitNode(middle, ImGuiDir_Left, .17f, nullptr, &middle);
    auto right = ImGui::DockBuilderSplitNode(middle, ImGuiDir_Right, .29f, nullptr, &middle);
    auto bottom = ImGui::DockBuilderSplitNode(middle, ImGuiDir_Down, .28f, nullptr, &middle);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Hierarchy"), left);
    auto materials = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, .40f, nullptr, &right);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Materials"), materials);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Inspector"), right);

    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Viewport"), middle);
    auto info = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, .28f, nullptr, &bottom);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Scene"), info);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Editor help"), info);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Assets"), bottom);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Console"), info);
    ImGui::DockBuilderDockWindow(EditorLocalization::Label("Shaders"), info);
    ImGui::DockBuilderFinish(id);
}
} // namespace
void EditorLayer::Initialize(const std::filesystem::path &path)
{
    EditorLocalization::Initialize();
    strncpy_s(mScenePath, PathText(path).c_str(), _TRUNCATE);
    LoadPreferences();
    Recent(path);
}
void EditorLayer::Shutdown()
{
    SavePreferences();
    ClearThumbnails();
    mMaterialPreview.reset();
    if (mAssetPreviewHandle.GetCpuPtr() != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
        Renderer::GetTextureHeap().Free(mAssetPreviewHandle);
    mAssetPreview = nullptr;
}
void EditorLayer::LoadPreferences()
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(Preferences().string().c_str()) != tinyxml2::XML_SUCCESS)
        return;
    if (auto root = doc.FirstChildElement("Editor"))
    {
        if (auto language = root->Attribute("language"))
            if (!EditorLocalization::SetLanguage(language))
                EditorLocalization::SetLanguage("en");
        mShowAssets = root->BoolAttribute("assets", true);
        mShowMaterials = root->BoolAttribute("materials", true);
        mShowCameraGizmos = root->BoolAttribute("cameraFrustums", true);
        mThumbnailView = root->BoolAttribute("thumbnails", true);
        mResetLayout = root->IntAttribute("layoutVersion") != 2;
#ifndef RELEASE
        mMaximizeOnPlay = root->BoolAttribute("maximizeOnPlay", false);
#endif
        mShowConsole = root->BoolAttribute("console", true);
        mShowShaders = root->BoolAttribute("shaders", false);
        for (auto n = root->FirstChildElement("Recent"); n && mRecentScenes.size() < 10;
             n = n->NextSiblingElement("Recent"))
            if (auto path = n->Attribute("path"))
                mRecentScenes.emplace_back(UTF8ToWString(path));
    }
}
void EditorLayer::SavePreferences()
{
    // Smoke runs disable the layout file; keep their transient panel/play choices
    // out of the user's saved workspace as well.
    if (ImGui::GetCurrentContext() && !ImGui::GetIO().IniFilename)
        return;
    tinyxml2::XMLDocument doc;
    auto root = doc.NewElement("Editor");
    doc.InsertEndChild(root);
    root->SetAttribute("assets", mShowAssets);
    root->SetAttribute("language", EditorLocalization::CurrentLanguage().c_str());
    root->SetAttribute("materials", mShowMaterials);
    root->SetAttribute("cameraFrustums", mShowCameraGizmos);
    root->SetAttribute("layoutVersion", 2);
    root->SetAttribute("thumbnails", mThumbnailView);
    root->SetAttribute("maximizeOnPlay", mMaximizeOnPlay);
    root->SetAttribute("console", mShowConsole);
    root->SetAttribute("shaders", mShowShaders);
    for (auto &p : mRecentScenes)
    {
        auto recent = doc.NewElement("Recent");
        recent->SetAttribute("path", PathText(p).c_str());
        root->InsertEndChild(recent);
    }
    if (doc.SaveFile(Preferences().string().c_str()) != tinyxml2::XML_SUCCESS)
        WriteLog(LogLevel::Warning, "Editor", "Cannot save editor preferences");
}
void EditorLayer::Recent(const std::filesystem::path &path)
{
    auto absolute = std::filesystem::absolute(path).lexically_normal();
    std::erase(mRecentScenes, absolute);
    mRecentScenes.insert(mRecentScenes.begin(), absolute);
    if (mRecentScenes.size() > 10)
        mRecentScenes.resize(10);
}
void EditorLayer::OpenScene(const std::filesystem::path &path)
{
    mPending = Pending::Open;
    mPendingPath = path;
}
void EditorLayer::Capture(Scene &s, const std::string &label)
{
    if (!mPlaying)
        Run([&] { mDocument.Capture(s.GetWorld(), s.GetName(), EditorActions::Id(s.GetWorld(), mSelected), label); });
}
void EditorLayer::Duplicate(World &w)
{
    auto clone = SceneSerializer::Duplicate(w, mSelected);
    if (auto camera = w.TryGet<CameraComponent>(clone))
        camera->primary = false;
    if (auto sun = w.TryGet<DirectionalLightComponent>(clone))
        sun->enabled = false;
    Select(w, clone);
}
void EditorLayer::Focus(Scene &s)
{
    Run([&] { mFocus = EditorActions::FrameSelection(s.GetWorld(), mSelected, s.GetCamera()); });
}
void EditorLayer::Undo(Scene &s, bool redo)
{
    if (mPlaying)
        return;
    Capture(s);
    Run([&] {
        auto &w = s.GetWorld();
        auto current = EditorActions::Id(w, mSelected);
        if (redo ? !mDocument.CanRedo() : !mDocument.CanUndo())
            return;
        DX12Core::gCommandManager.IdleGPU();
        auto selected = redo ? mDocument.Redo(w) : mDocument.Undo(w);
        s.SetName(mDocument.SceneName());
        auto e = EditorActions::Find(w, current);
        Select(w, w.IsValid(e) ? e : EditorActions::Find(w, selected));
    });
}
bool EditorLayer::SaveDocument(Scene &s, bool saveAs)
{
    if (mPlaying)
        return false;
    auto path = std::filesystem::path(UTF8ToWString(mScenePath));
    if (saveAs || path.empty())
    {
        path = SceneDialog(true, mScenePath);
        if (path.empty())
            return false;
    }
    bool saved = false;
    Run([&] {
        Capture(s);
        s.Save(path);
        mDocument.MarkSaved(s.GetWorld(), s.GetName());
        strncpy_s(mScenePath, PathText(path).c_str(), _TRUNCATE);
        Recent(path);
        saved = true;
    });
    return saved;
}
void EditorLayer::ExecutePending(Scene &s)
{
    auto operation = mPending;
    auto path = mPendingPath;
    mPending = Pending::None;
    mModal = false;
    Run([&] {
        if (operation == Pending::Exit)
        {
            mExitRequested = true;
            return;
        }
        DX12Core::gCommandManager.IdleGPU();
        if (operation == Pending::New)
        {
            World empty;
            s.GetWorld().ReplaceWith(empty);
            EnsureSceneViewObjects(s.GetWorld(), s.GetCamera());
            s.SetName("Untitled");
            mScenePath[0] = 0;
        }
        else if (operation == Pending::Open)
        {
            mPreloadPath = path;
            mPreloadSnapshot = SceneSerializer::ToXml(s.GetWorld(), s.GetName());
            mScenePreload = AssetManager::PreloadAsync(SceneSerializer::CollectResources(path));
            return;
        }
        Select(s.GetWorld(), entt::null);
        mDocument.Reset(s.GetWorld(), s.GetName());
        mDocumentReady = true;
    });
}
void EditorLayer::ConfirmPending(Scene &s)
{
    if (mPending == Pending::None)
        return;
    if (mPlaying)
    {
        Run([&] { TogglePlay(s); });
        if (mPlaying)
        {
            mPending = Pending::None;
            return;
        }
    }
    Capture(s);
    if (!mDocument.Dirty())
    {
        ExecutePending(s);
        return;
    }
    mModal = true;
    ImGui::OpenPopup(EditorLocalization::Label("Unsaved scene"));
    if (ImGui::BeginPopupModal(EditorLocalization::Label("Unsaved scene"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(EditorLocalization::Text("The scene has unsaved changes."));
        if (ImGui::Button(EditorLocalization::Label("Save"), ImVec2(100, 0)))
        {
            if (SaveDocument(s))
            {
                ImGui::CloseCurrentPopup();
                ExecutePending(s);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(EditorLocalization::Label("Discard"), ImVec2(100, 0)))
        {
            ImGui::CloseCurrentPopup();
            ExecutePending(s);
        }
        ImGui::SameLine();
        if (ImGui::Button(EditorLocalization::Label("Cancel"), ImVec2(100, 0)))
        {
            mPending = Pending::None;
            mModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void EditorLayer::Shortcuts(Scene &s)
{
    auto &io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsAnyItemActive() || mModal ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;
    if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            SaveDocument(s, io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_O, false) && !mPlaying)
        {
            auto path = SceneDialog(false, mScenePath);
            if (!path.empty())
                OpenScene(path);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_N, false) && !mPlaying)
            mPending = Pending::New;
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            Undo(s, io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            Undo(s, true);
        if (ImGui::IsKeyPressed(ImGuiKey_D, false) && !mPlaying)
            Run([&] { Duplicate(s.GetWorld()); });
    }
    else
    {
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            Focus(s);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !mPlaying)
            Run([&] {
                EditorActions::DeleteTree(s.GetWorld(), mSelected);
                Select(s.GetWorld(), entt::null);
            });
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            Select(s.GetWorld(), entt::null);
        if (mViewportHovered && !io.KeyShift && !io.KeyAlt && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false))
                mGizmoOperation = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false))
                mGizmoOperation = 2;
            if (ImGui::IsKeyPressed(ImGuiKey_S, false))
                mGizmoOperation = 3;
        }
    }
}
void EditorLayer::Workspace(Scene &s)
{
    Shortcuts(s);
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::TextColored(EditorTheme::Accent(), "ATOM");
        ImGui::Separator();
        if (ImGui::BeginMenu(EditorLocalization::Label("File")))
        {
            if (ImGui::MenuItem(EditorLocalization::Label("New scene"), "Ctrl+N", false, !mPlaying))
                mPending = Pending::New;
            if (ImGui::MenuItem(EditorLocalization::Label("Open scene..."), "Ctrl+O", false, !mPlaying))
            {
                auto path = SceneDialog(false, mScenePath);
                if (!path.empty())
                    OpenScene(path);
            }
            if (ImGui::BeginMenu(EditorLocalization::Label("Recent scenes"), !mPlaying))
            {
                for (auto &p : mRecentScenes)
                    if (ImGui::MenuItem(PathText(p).c_str()))
                        OpenScene(p);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(EditorLocalization::Label("Save"), "Ctrl+S", false, !mPlaying))
                SaveDocument(s);
            if (ImGui::MenuItem(EditorLocalization::Label("Save As..."), "Ctrl+Shift+S", false, !mPlaying))
                SaveDocument(s, true);
            ImGui::Separator();
            if (ImGui::MenuItem(EditorLocalization::Label("Exit")))
                RequestClose();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(EditorLocalization::Label("Edit")))
        {
            if (ImGui::MenuItem(EditorLocalization::Label("Undo"), "Ctrl+Z", false, !mPlaying && mDocument.CanUndo()))
                Undo(s);
            if (ImGui::MenuItem(EditorLocalization::Label("Redo"), "Ctrl+Y", false, !mPlaying && mDocument.CanRedo()))
                Undo(s, true);
            if (ImGui::MenuItem(EditorLocalization::Label("Duplicate subtree"), "Ctrl+D", false,
                                !mPlaying && s.GetWorld().IsValid(mSelected)))
                Run([&] { Duplicate(s.GetWorld()); });
            if (ImGui::MenuItem(EditorLocalization::Label("Frame selected"), "F", false,
                                s.GetWorld().IsValid(mSelected)))
                Focus(s);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(EditorLocalization::Label("View")))
        {
            ImGui::MenuItem(EditorLocalization::Label("Assets"), nullptr, &mShowAssets);
            ImGui::MenuItem(EditorLocalization::Label("Materials"), nullptr, &mShowMaterials);
            ImGui::MenuItem(EditorLocalization::Label("Console"), nullptr, &mShowConsole);
            ImGui::MenuItem(EditorLocalization::Label("Shaders"), nullptr, &mShowShaders);
            ImGui::MenuItem(EditorLocalization::Label("Camera frustums"), nullptr, &mShowCameraGizmos);
            ImGui::MenuItem(EditorLocalization::Label("Grid"), nullptr, &gContext.showGrid);
            ImGui::MenuItem(EditorLocalization::Label("Colliders"), nullptr, &gContext.showColliders);
            ImGui::MenuItem(EditorLocalization::Label("Render diagnostics"), nullptr, &gContext.showRenderDiagnostics);
            if (ImGui::MenuItem(EditorLocalization::Label("Reset layout")))
                mResetLayout = true;
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(EditorLocalization::Label("Help")))
            mShowHelp = !mShowHelp;
        if (ImGui::BeginMenu(EditorLocalization::Label("Language")))
        {
            for (const auto &language : EditorLocalization::Languages())
                if (ImGui::MenuItem(language.name.c_str(), nullptr,
                                    language.id == EditorLocalization::CurrentLanguage()))
                {
                    EditorLocalization::SetLanguage(language.id);
                    SavePreferences();
                }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::TextDisabled("%s%s", s.GetName().c_str(), mDocument.Dirty() ? " *" : "");
        ImGui::EndMainMenuBar();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 3));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorTheme::Gray(26));
    if (ImGui::BeginViewportSideBar("##Editor status", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(),
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                        ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImGui::TextColored(mPlaying ? (mPaused ? EditorTheme::Color(217, 172, 87) : EditorTheme::Color(112, 192, 138))
                                    : EditorTheme::Accent(),
                           "%s", EditorLocalization::Text(mPlaying ? (mPaused ? "PAUSED" : "PLAYING") : "EDITOR"));
        ImGui::SameLine();
        ImGui::TextDisabled(" | %s",
                            EditorLocalization::Text(mDocument.Dirty() ? "Unsaved changes" : "All changes saved"));
        if (ImGui::GetContentRegionAvail().x > 300)
        {
            ImGui::SameLine();
            auto name = s.GetWorld().TryGet<NameComponent>(mSelected);
            ImGui::TextDisabled(" | %s", name ? name->value.c_str() : EditorLocalization::Text("No selection"));
        }
        const float fpsWidth = ImGui::CalcTextSize("9999 FPS | 999.9 ms").x;
        if (ImGui::GetContentRegionAvail().x > fpsWidth + 20)
        {
            ImGui::SameLine(ImGui::GetWindowWidth() - fpsWidth - 12);
            auto fps = ImGui::GetIO().Framerate;
            ImGui::TextDisabled("%.0f FPS | %.1f ms", fps, fps > 0 ? 1000.f / fps : 0.f);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGuiWindowClass docking;
    docking.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    auto dock = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None, &docking);
    auto node = ImGui::DockBuilderGetNode(dock);
    if (mResetLayout || (node && !node->IsSplitNode() && node->Windows.Size == 0))
    {
        DockLayout(dock);
        mResetLayout = false;
    }
}
void EditorLayer::Viewport(Scene &s)
{
    ImGui::Begin(EditorLocalization::Label("Viewport"));
    s.GetCamera().SetAspectRatio(float(gSceneColorBuffer.GetHeight()) / std::max(1u, gSceneColorBuffer.GetWidth()));
    PlayToolbar(s);
    const char *names[] = {"Move [T]", "Rotate [R]", "Scale [S]"};
    for (int i = 0; i < 3; ++i)
    {
        if (i)
            ImGui::SameLine();
        if (EditorTheme::ToggleButton(EditorLocalization::Label(names[i]), mGizmoOperation == i + 1, ImVec2(94, 0)))
            mGizmoOperation = i + 1;
        EditorTheme::Tooltip(EditorLocalization::Text(i == 0   ? "Translate selected object (T)"
                                                      : i == 1 ? "Rotate selected object (R)"
                                                               : "Scale selected object (S)"));
    }
    ImGui::Checkbox(EditorLocalization::Label("Local"), &mLocal);
    ImGui::SameLine();
    ImGui::Checkbox(EditorLocalization::Label("Snap"), &mSnap);
    ImGui::SameLine();
    float *snapStep = mGizmoOperation == 2 ? &mRotateSnap : mGizmoOperation == 3 ? &mScaleSnap : &mMoveSnap;
    ImGui::SetNextItemWidth(70);
    ImGui::DragFloat(EditorLocalization::Label("Step##TransformSnapStep"), snapStep, .05f, .001f, 100, "%.2f",
                     ImGuiSliderFlags_AlwaysClamp);
    ImGui::Separator();
    auto available = ImGui::GetContentRegionAvail();
    float aspect = float(gSceneColorBuffer.GetWidth()) / std::max(1u, gSceneColorBuffer.GetHeight());
    ImVec2 size(available.x, available.x / aspect);
    if (size.y > available.y)
        size = ImVec2(available.y * aspect, available.y);
    mViewportHovered = false;
    mGizmoActive = false;
    if (mViewportTexture && size.x > 1 && size.y > 1)
    {
        auto cursor = ImGui::GetCursorScreenPos();
        cursor.x += (available.x - size.x) * .5f;
        cursor.y += (available.y - size.y) * .5f;
        ImGui::SetCursorScreenPos(cursor);
        ImGuiCommon::ImageDisplayEncoded((ImTextureID)mViewportTexture, size);
        mViewportHovered = ImGui::IsItemHovered();
        auto mouse = ImGui::GetMousePos();
        mRuntimePointer = Vector2((mouse.x - cursor.x) * gSceneColorBuffer.GetWidth() / size.x,
                                  (mouse.y - cursor.y) * gSceneColorBuffer.GetHeight() / size.y);
        if (!mPlaying && ImGui::BeginDragDropTarget())
        {
            if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
            {
                auto path = std::string(static_cast<const char *>(payload->Data));
                auto ext = Extension(UTF8ToWString(path));
                if (ext == ".mat")
                    Run([&] {
                        auto target = SelectionRenderer::Pick(s.GetWorld(), s.GetCamera(), mRuntimePointer.x,
                                                              mRuntimePointer.y, float(gSceneColorBuffer.GetWidth()),
                                                              float(gSceneColorBuffer.GetHeight()));
                        ApplyMaterial(s.GetWorld(), target, UTF8ToWString(path));
                        Select(s.GetWorld(), target);
                    });
                else if (ext == ".scene")
                    OpenScene(UTF8ToWString(path));
                else if (ext == ".prefab")
                    Run([&] {
                        Select(s.GetWorld(), SceneSerializer::InstantiatePrefab(s.GetWorld(), UTF8ToWString(path)));
                    });
                else if (ModelFile(UTF8ToWString(path)))
                    Run([&] {
                        auto model = AssetManager::LoadModel(path);
                        if (!model)
                            throw std::runtime_error("Cannot load model: " + path);
                        auto o =
                            s.GetWorld().CreateGameObject(PathText(std::filesystem::path(UTF8ToWString(path)).stem()));
                        o->AddComponent<TransformComponent>();
                        o->AddComponent<MeshSourceComponent>(BuiltinMesh::None, path);
                        o->AddComponent<MaterialComponent>(model);
                        o->AddComponent<MeshComponent>(model);
                        Select(s.GetWorld(), o->GetHandle());
                    });
            }
            ImGui::EndDragDropTarget();
        }
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(cursor.x, cursor.y, size.x, size.y);
        bool cameraGesture = ImGui::GetIO().KeyAlt || ImGui::GetIO().KeyShift ||
                             ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsMouseDown(ImGuiMouseButton_Right);
        ImGuizmo::Enable(!mPlaying && !mModal && mGizmoOperation != 0 && !cameraGesture);
        auto t = s.GetWorld().TryGet<TransformComponent>(mSelected);
        if (t && !mPlaying && mGizmoOperation != 0)
        {
            auto view = s.GetCamera().GetViewMatrix();
            auto camera = s.GetCamera();
            camera.ReverseZ(false);
            auto projection = camera.GetProjMatrix();
            auto matrix = t->GetMatrix();
            float snap[3] = {*snapStep, *snapStep, *snapStep};
            auto op = mGizmoOperation == 1   ? ImGuizmo::TRANSLATE
                      : mGizmoOperation == 2 ? ImGuizmo::ROTATE
                                             : ImGuizmo::SCALE;
            ImGui::GetWindowDrawList()->PushClipRect(cursor, ImVec2(cursor.x + size.x, cursor.y + size.y), true);
            if (ImGuizmo::Manipulate(&view[0][0], &projection[0][0], op, mLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                                     &matrix[0][0], nullptr, mSnap ? snap : nullptr))
                Run([&] { EditorActions::SetWorldMatrix(s.GetWorld(), mSelected, matrix); });
            mGizmoActive = !cameraGesture && (ImGuizmo::IsUsing() || ImGuizmo::IsOver());
            ImGui::GetWindowDrawList()->PopClipRect();
        }
        if (mViewportHovered && !mGizmoActive && !mPlaying && !mModal && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::GetIO().KeyAlt)
            Select(s.GetWorld(),
                   SelectionRenderer::Pick(s.GetWorld(), s.GetCamera(), mRuntimePointer.x, mRuntimePointer.y,
                                           float(gSceneColorBuffer.GetWidth()), float(gSceneColorBuffer.GetHeight())));
    }
    ImGui::End();
}
void EditorLayer::Assets(Scene &s)
{
    ImGui::Begin(EditorLocalization::Label("Assets"), &mShowAssets);
    if (ImGui::Button(EditorLocalization::Label("Assets")))
    {
        mAssetDirectory = "Asset";
        mRefreshAssets = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Materials")))
    {
        mAssetDirectory = "Asset/Materials";
        mRefreshAssets = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Scripts")))
    {
        mAssetDirectory = Preferences().parent_path() / "Scripts";
        mRefreshAssets = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Up")) && mAssetDirectory.has_parent_path())
    {
        mAssetDirectory = mAssetDirectory.parent_path();
        mRefreshAssets = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Refresh")))
        mRefreshAssets = true;
    if (ImGui::GetContentRegionAvail().x > 280)
        ImGui::SameLine();
    ScriptCreation(s.GetWorld());
    ImGui::Spacing();
    ImGui::TextDisabled("%s", PathText(mAssetDirectory).c_str());
    static ImGuiTextFilter search;
    const bool wideSearch = ImGui::GetContentRegionAvail().x > 400;
    ImGui::SetNextItemWidth(wideSearch ? ImGui::GetContentRegionAvail().x - 230 : -1);
    if (ImGui::InputTextWithHint("##Search assets", EditorLocalization::Text("Search assets..."), search.InputBuf,
                                 sizeof(search.InputBuf)))
        search.Build();
    EditorTheme::Tooltip(EditorLocalization::Text("Filter assets by name; use commas for multiple filters"));
    if (wideSearch)
        ImGui::SameLine();
    ImGui::Checkbox(EditorLocalization::Label("Thumbnails"), &mThumbnailView);
    ImGui::SameLine();
    ImGui::TextDisabled(EditorLocalization::Text("%zu items"), mAssetEntries.size());
    if (mRefreshAssets)
    {
        ClearThumbnails();
        mRefreshAssets = false;
        mAssetEntries.clear();
        std::error_code error;
        for (std::filesystem::directory_iterator it(mAssetDirectory, error), end; !error && it != end;
             it.increment(error))
            if (it->is_directory(error) || (!error && it->is_regular_file(error)))
                mAssetEntries.push_back(it->path());
        std::sort(mAssetEntries.begin(), mAssetEntries.end(), [](auto &a, auto &b) {
            std::error_code ec;
            bool af = std::filesystem::is_directory(a, ec), bf = std::filesystem::is_directory(b, ec);
            if (af != bf)
                return af;
            return a.filename() < b.filename();
        });
        if (error)
            WriteLog(LogLevel::Warning, "Editor", "Cannot browse assets: " + error.message());
    }
    ImGui::Separator();
    int thumbnailBudget = 2;
    size_t pendingThumbnails = 0;
    for (auto &[key, thumbnail] : mThumbnails)
        if (thumbnail.pending)
        {
            ++pendingThumbnails;
            if (thumbnailBudget <= 0)
                continue;
            auto before = thumbnail.pending->Report().completed;
            const auto &report = thumbnail.pending->Step(1);
            if (report.completed != before)
                --thumbnailBudget;
            if (!report.Ready())
                continue;
            try
            {
                if (!report.Success())
                    throw std::runtime_error("Thumbnail preload failed");
                thumbnail.texture = AssetManager::LoadCovertTexture(thumbnail.path, kWhiteOpaque2D, true);
                if (!thumbnail.texture.IsValid() ||
                    const_cast<ID3D12Resource *>(thumbnail.texture->GetResource())->GetDesc().DepthOrArraySize != 1)
                    thumbnail.failed = true;
                else
                {
                    thumbnail.handle = Renderer::GetTextureHeap().Alloc();
                    DX12Core::gDevice->CopyDescriptorsSimple(1, thumbnail.handle, thumbnail.texture.GetSRV(),
                                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
            }
            catch (const std::exception &)
            {
                thumbnail.failed = true;
            }
            thumbnail.pending.reset();
            --pendingThumbnails;
        }
    ImGui::BeginChild("Asset list", ImVec2(mAssetSelection.empty() ? 0 : ImGui::GetContentRegionAvail().x * .65f, 0));
    bool grid =
        mThumbnailView && ImGui::BeginTable("Asset grid", std::max(1, int(ImGui::GetContentRegionAvail().x / 116.f)),
                                            ImGuiTableFlags_SizingFixedFit);
    for (auto &entry : mAssetEntries)
    {
        auto name = PathText(entry.filename());
        if (!search.PassFilter(name.c_str()))
            continue;
        std::error_code statusError;
        bool folder = std::filesystem::is_directory(entry, statusError);
        auto path = PathText(entry);
        if (std::filesystem::absolute(entry).parent_path() == Preferences().parent_path() / "Scripts")
            path = "Scripts/" + name;
        ImGui::PushID(path.c_str());
        bool clicked = false;
        if (grid)
        {
            ImGui::TableNextColumn();
            auto pos = ImGui::GetCursorScreenPos();
            clicked = ImGui::InvisibleButton("Asset tile", ImVec2(108, 100));
            auto draw = ImGui::GetWindowDrawList();
            auto color = mAssetSelection == path  ? EditorTheme::U32(40, 70, 94)
                         : ImGui::IsItemHovered() ? EditorTheme::U32(52, 52, 52)
                                                  : EditorTheme::U32(29, 29, 29);
            draw->AddRectFilled(pos, ImVec2(pos.x + 108, pos.y + 100), color, 5);
            if (folder)
            {
                draw->AddRectFilled(ImVec2(pos.x + 18, pos.y + 20), ImVec2(pos.x + 50, pos.y + 31),
                                    EditorTheme::U32(226, 175, 66), 3);
                draw->AddRectFilled(ImVec2(pos.x + 18, pos.y + 28), ImVec2(pos.x + 90, pos.y + 64),
                                    EditorTheme::U32(234, 187, 82), 4);
            }
            else
            {
                auto key = PathText(std::filesystem::absolute(entry));
                auto found = mThumbnails.find(key);
                if (TextureFile(entry) && found == mThumbnails.end() && thumbnailBudget > 0 &&
                    mThumbnails.size() < 128 && pendingThumbnails < 4 && ImGui::IsItemVisible())
                {
                    --thumbnailBudget;
                    Thumbnail thumbnail;
                    try
                    {
                        thumbnail.path = path;
                        thumbnail.pending = AssetManager::PreloadAsync(
                            {{AssetKind::Texture, path, BuiltinMesh::None, kWhiteOpaque2D, true}});
                        thumbnail.pending->Step(1);
                        ++pendingThumbnails;
                    }
                    catch (const std::exception &)
                    {
                        thumbnail.failed = true;
                    }
                    found = mThumbnails.emplace(key, std::move(thumbnail)).first;
                }
                if (found != mThumbnails.end() && !found->second.failed && !found->second.pending &&
                    found->second.texture.IsValid())
                {
                    auto &thumbnail = found->second;
                    float aspect = float(thumbnail.texture.Width()) / std::max(1u, thumbnail.texture.Height());
                    ImVec2 image(aspect >= 1 ? 72.f : 72.f * aspect, aspect >= 1 ? 72.f / aspect : 72.f);
                    auto top = ImVec2(pos.x + (108 - image.x) * .5f, pos.y + 4 + (72 - image.y) * .5f);
                    draw->AddImage((ImTextureID)thumbnail.handle.GetGpuPtr(), top,
                                   ImVec2(top.x + image.x, top.y + image.y));
                }
                else
                {
                    draw->AddRectFilled(ImVec2(pos.x + 27, pos.y + 12), ImVec2(pos.x + 81, pos.y + 77),
                                        EditorTheme::U32(63, 63, 63), 3);
                    auto extension = Extension(entry);
                    draw->AddText(ImVec2(pos.x + 32, pos.y + 36), EditorTheme::U32(218, 229, 240), extension.c_str());
                }
            }
            std::string label = name;
            while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > 98)
            {
                size_t last = label.size() - 1;
                while (last > 0 && (uint8_t(label[last]) & 0xc0) == 0x80)
                    --last;
                label.erase(last);
            }
            draw->AddText(ImVec2(pos.x + 5, pos.y + 80), EditorTheme::U32(226, 232, 240), label.c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", name.c_str());
        }
        else
            clicked = ImGui::Selectable((folder ? "[Folder] " + name : name).c_str(), mAssetSelection == path);
        if (clicked)
        {
            mAssetSelection = path;
            if (!folder && Extension(entry) == ".mat")
                Run([&] { SelectMaterial(entry); });
            mAssetPreview = nullptr;
            if (!folder && TextureFile(entry))
                Run([&] {
                    mAssetPreview = AssetManager::LoadCovertTexture(PathText(entry), kWhiteOpaque2D, true);
                    if (!mAssetPreview.IsValid())
                        throw std::runtime_error("Cannot preview texture");
                    auto handle = Renderer::GetTextureHeap().Alloc();
                    DX12Core::gDevice->CopyDescriptorsSimple(1, handle, mAssetPreview.GetSRV(),
                                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    if (!mAssetPreviewHandle.IsNull())
                        Renderer::GetTextureHeap().Free(mAssetPreviewHandle);
                    mAssetPreviewHandle = handle;
                });
        }
        if (!folder && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("ATOM_ASSET", path.c_str(), path.size() + 1);
            ImGui::TextUnformatted(name.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (folder)
            {
                mAssetDirectory = entry;
                mRefreshAssets = true;
            }
            else if (Extension(entry) == ".scene" && !mPlaying)
                OpenScene(UTF8ToWString(path));
            else if (Extension(entry) == ".prefab" && !mPlaying)
                Run([&] {
                    Select(s.GetWorld(), SceneSerializer::InstantiatePrefab(s.GetWorld(), UTF8ToWString(path)));
                });
        }
        ImGui::PopID();
    }
    if (grid)
        ImGui::EndTable();
    ImGui::EndChild();
    if (!mAssetSelection.empty())
    {
        ImGui::SameLine();
        ImGui::BeginChild("Asset details");
        ImGui::TextWrapped("%s", mAssetSelection.c_str());
        if (ImGui::Button(EditorLocalization::Label("Copy path")))
            ImGui::SetClipboardText(mAssetSelection.c_str());
        if (mAssetPreview.IsValid())
        {
            float width = std::min(180.f, ImGui::GetContentRegionAvail().x);
            ImGui::Image((ImTextureID)mAssetPreviewHandle.GetGpuPtr(),
                         ImVec2(width, width * float(mAssetPreview.Height()) / std::max(1u, mAssetPreview.Width())));
            auto materials = s.GetWorld().TryGet<MaterialComponent>(mSelected);
            ImGui::BeginDisabled(mPlaying || !materials || materials->mMaterials.empty());
            if (ImGui::Button(EditorLocalization::Label("Use as BaseColor")))
                materials->mMaterials[0].texturePaths[0] = mAssetSelection;
            ImGui::EndDisabled();
        }
        if (ImGui::Button(EditorLocalization::Label("Open externally")))
        {
            auto path = std::filesystem::path(UTF8ToWString(mAssetSelection));
            if (mAssetSelection.starts_with("Scripts/"))
                path = Preferences().parent_path() / path;
            ShellExecuteW(gContext.windowManager->GetWindowHandle(), L"open", path.c_str(), nullptr, nullptr,
                          SW_SHOWNORMAL);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
} // namespace AtomEngine
