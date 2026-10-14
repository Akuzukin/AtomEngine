#include "Runtime/Function/Audio/Audio.h"
#include "EditorPanelSupport.h"
#include "Runtime/Function/Scene/SceneAuthoring.h"
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
        if (ActiveProject.project) ActiveProject.currentScene = WStringToUTF8(std::filesystem::absolute(path).lexically_relative(ActiveProject.project->Root()).generic_wstring());
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
        if (operation == Pending::NewProject || operation == Pending::OpenProject)
        {
            DX12Core::gCommandManager.IdleGPU();
            mProjectPanel.Activate(*this, s, path, operation == Pending::NewProject);
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
        else if(operation==Pending::Template) {
            s.SetName(SceneSerializer::Load(s.GetWorld(),path)+" copy");EnsureSceneViewObjects(s.GetWorld(),s.GetCamera());mScenePath[0]=0;
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
        if(operation==Pending::Template)mDocument.MarkUnsaved();
        mDocumentReady = true;
    });
}
void EditorLayer::ConfirmPending(Scene &s)
{
    if (mPending == Pending::None)
        return;
    if (mProjectPanel.Busy()) return;
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
    bool projectDirty = ActiveProject.dirty && (mPending == Pending::NewProject || mPending == Pending::OpenProject || mPending == Pending::Exit);
    if (!mDocument.Dirty() && !projectDirty)
    {
        ExecutePending(s);
        return;
    }
    mModal = true;
    ImGui::OpenPopup(EditorLocalization::Label("Unsaved changes"));
    if (ImGui::BeginPopupModal(EditorLocalization::Label("Unsaved changes"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (mDocument.Dirty()) ImGui::TextUnformatted(EditorLocalization::Text("The scene has unsaved changes."));
        if (projectDirty) ImGui::TextUnformatted(EditorLocalization::Text("The project has unsaved changes."));
        if (ImGui::Button(EditorLocalization::Label("Save"), ImVec2(100, 0)))
        {
            bool saved = !mDocument.Dirty() || SaveDocument(s);
            if (saved && projectDirty) { saved = false; Run([&] { ActiveProject.project->Save(); ActiveProject.dirty = false; saved = true; }); }
            if (saved)
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
    if (mPlaying) return;
    auto &io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsAnyItemActive() || mModal ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;
    if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_S, false))
            { if (io.KeyAlt) mProjectPanel.Save(*this, s); else SaveDocument(s, io.KeyShift); }
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



} // namespace AtomEngine
