#include "EditorLayer.h"
#include "EditorTheme.h"
#include "EditorLocalization.h"
#include "ScriptTools.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Input/Input.h"
#include "Runtime/Function/Render/SelectionRenderer.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Resource/Shader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace AtomEngine
{
namespace
{
bool Text(const char *label, std::string &s, float reserve = 0)
{
    char buffer[1024]{};
    strncpy_s(buffer, s.c_str(), _TRUNCATE);
    ImGui::TextUnformatted(EditorLocalization::Text(label));
    ImGui::SetNextItemWidth(-1 - reserve);
    if (ImGui::InputText((std::string("##") + label).c_str(), buffer, sizeof(buffer)))
    {
        s = buffer;
        return true;
    }
    return false;
}
bool AssetPath(const char *label, std::string &value, const char *extensions)
{
    bool changed = Text(label, value);
    if (ImGui::BeginDragDropTarget())
    {
        if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
        {
            auto path = static_cast<const char *>(payload->Data);
            auto ext = std::filesystem::path(UTF8ToWString(path)).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (std::string(extensions).find("|" + ext + "|") != std::string::npos)
            {
                value = path;
                changed = true;
            }
        }
        ImGui::EndDragDropTarget();
    }
    return changed;
}
bool Float(const char *label, float *value, float speed = 1.f, float minimum = 0.f, float maximum = 0.f,
           const char *format = "%.3f", ImGuiSliderFlags flags = 0)
{
    ImGui::TextUnformatted(EditorLocalization::Text(label));
    ImGui::SetNextItemWidth(-1);
    return ImGui::DragFloat((std::string("##") + label).c_str(), value, speed, minimum, maximum, format, flags);
}
bool Float2(const char *label, float *value, float speed = 1.f, float minimum = 0.f, float maximum = 0.f,
            const char *format = "%.3f", ImGuiSliderFlags flags = 0)
{
    ImGui::TextUnformatted(EditorLocalization::Text(label));
    ImGui::SetNextItemWidth(-1);
    return ImGui::DragFloat2((std::string("##") + label).c_str(), value, speed, minimum, maximum, format, flags);
}
bool Float3(const char *label, float *value, float speed = 1.f, float minimum = 0.f, float maximum = 0.f,
            const char *format = "%.3f", ImGuiSliderFlags flags = 0)
{
    ImGui::TextUnformatted(EditorLocalization::Text(label));
    ImGui::SetNextItemWidth(-1);
    return ImGui::DragFloat3((std::string("##") + label).c_str(), value, speed, minimum, maximum, format, flags);
}
template <class... Args> bool Color3(const char *label, Args &&...args)
{
    ImGui::TextUnformatted(EditorLocalization::Text(label));
    ImGui::SetNextItemWidth(-1);
    return ImGui::ColorEdit3((std::string("##") + label).c_str(), std::forward<Args>(args)...);
}
template <class... Args> bool Color4(const char *label, Args &&...args)
{
    ImGui::TextUnformatted(EditorLocalization::Text(label));
    ImGui::SetNextItemWidth(-1);
    return ImGui::ColorEdit4((std::string("##") + label).c_str(), std::forward<Args>(args)...);
}
void Rotation(const char *label, Quaternion &q)
{
    Matrix4x4 matrix;
    matrix.MakeAffine(Vector3::UNIT_SCALE, q, Vector3::ZERO);
    float position[3], angles[3], scale[3];
    ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], position, angles, scale);
    if (Float3(label, angles, .5f))
    {
        ImGuizmo::RecomposeMatrixFromComponents(position, angles, scale, &matrix[0][0]);
        q = matrix.GetRotation();
    }
}
void TransformEditor(Transform &t)
{
    Float3("Position", t.transition.ptr(), .05f);
    Rotation("Rotation (degrees)", t.rotation);
    if (ImGui::SmallButton(EditorLocalization::Label("Reset transform")))
    {
        t.transition = Vector3::ZERO;
        t.rotation = Quaternion::IDENTITY;
        t.scale = Vector3::UNIT_SCALE;
    }
    Float3("Scale", t.scale.ptr(), .01f, .001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
}
} // namespace
void EditorLayer::Run(const std::function<void()> &action)
{
    try
    {
        action();
    }
    catch (const std::exception &e)
    {
        mError = e.what();
        WriteLog(LogLevel::Error, "Editor", mError);
    }
}
void EditorLayer::Select(World &w, Entity e)
{
    mWorld = w.GetLifetime();
    mSelected = e;
    mModelDraftId.clear();
    SelectionRenderer::Select(&w, e);
}
void EditorLayer::Create(World &w, BuiltinMesh type)
{
    auto o = w.CreateGameObject(type == BuiltinMesh::None ? "Object" : "Primitive");
    o->AddComponent<TransformComponent>();
    if (type != BuiltinMesh::None)
    {
        o->AddComponent<MeshSourceComponent>(type);
        SceneSerializer::RebuildMesh(w, o->GetHandle());
    }
    Select(w, o->GetHandle());
}
void EditorLayer::TogglePlay(Scene &s)
{
    if (!mPlaying)
    {
        Capture(s);
        mPlayXml = SceneSerializer::ToXml(s.GetWorld(), s.GetName());
        mPlaySelection = EditorActions::Id(s.GetWorld(), mSelected);
        mEditCamera = s.GetCamera();
        mPlaying = true;
        mPaused = false;
    }
    else
    {
        DX12Core::gCommandManager.IdleGPU();
        s.SetName(SceneSerializer::FromXml(s.GetWorld(), mPlayXml));
        mDocument.SyncRestoredState(s.GetWorld());
        mPlaying = false;
        mPaused = false;
        mStep = false;
        if (mEditCamera)
        {
            s.GetCamera() = *mEditCamera;
            mEditCamera.reset();
        }
        Select(s.GetWorld(), EditorActions::Find(s.GetWorld(), mPlaySelection));
        mPlayXml.clear();
    }
}
void EditorLayer::Update(Scene &s)
{
    PollBackgroundWork(s);
    mStep = false;
    if (!mDocumentReady)
    {
        mDocument.Reset(s.GetWorld(), s.GetName());
        mDocumentReady = true;
    }
    auto &w = s.GetWorld();
    if (mWorld.lock() != w.GetLifetime().lock() || !w.IsValid(mSelected))
        Select(w, entt::null);
    Workspace(s);
    if (mPlaying && mMaximizeOnPlay)
    {
        PlayViewport(s);
        ConfirmPending(s);
        mTextInput = ImGui::GetIO().WantTextInput;
        return;
    }
    if (mShowConsole)
    {
        ImGui::Begin(EditorLocalization::Label("Console"));
        static int minimum = 2;
        static ImGuiTextFilter filter;
        ImGui::SetNextItemWidth(160);
        EditorLocalization::Combo("Level", &minimum, "Trace\0Debug\0Info\0Warning\0Error\0Critical\0");
        if (ImGui::Button(EditorLocalization::Label("Clear")))
            ClearLogHistory();
        ImGui::SameLine();
        if (ImGui::Button(EditorLocalization::Label("Flush file")))
            FlushLog();
        filter.Draw("Filter");
        static bool autoScroll = true;
        ImGui::Checkbox(EditorLocalization::Label("Auto scroll"), &autoScroll);
        auto file = GetLogFile().string();
        ImGui::TextWrapped("%s", file.c_str());
        ImGui::BeginChild("Messages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        bool bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        for (auto &entry : GetLogHistory())
        {
            if (int(entry.level) < minimum || !filter.PassFilter(entry.formatted.c_str()))
                continue;
            if (entry.level >= LogLevel::Error)
                ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Color(231, 111, 111));
            else if (entry.level == LogLevel::Warning)
                ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::Color(221, 183, 100));
            ImGui::TextUnformatted(entry.formatted.c_str());
            if (entry.level >= LogLevel::Warning)
                ImGui::PopStyleColor();
        }
        if (autoScroll && bottom)
            ImGui::SetScrollHereY(1);
        ImGui::EndChild();
        ImGui::End();
    }
    Hierarchy(w);
    Inspector(s);
    if (mShowAssets)
        Assets(s);
    if (mShowMaterials)
        Materials(s);
    Viewport(s);
    if (mShowShaders)
    {
        ImGui::Begin(EditorLocalization::Label("Shaders"), &mShowShaders);
        if (!ShaderLibrary::Error().empty())
            ImGui::TextWrapped("%s", ShaderLibrary::Error().c_str());
        for (auto &shader : ShaderLibrary::All())
        {
            auto name = WStringToUTF8(shader->File());
            ImGui::PushID(shader.get());
            if (ImGui::TreeNode(name.c_str()))
            {
                ImGui::Text(EditorLocalization::Text("Revision: %llu"), shader->Revision());
                for (auto &binding : shader->Bindings())
                    ImGui::Text(EditorLocalization::Text("%s : register %u, space %u"), binding.name.c_str(),
                                binding.bindPoint, binding.space);
                ImGui::BeginDisabled(!shader->preview);
                if (ImGui::Button(EditorLocalization::Label("Preview")))
                    shader->RequestPreview({});
                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::End();
    }
    if (mShowHelp)
    {
        ImGui::Begin(EditorLocalization::Label("Editor help"), &mShowHelp);
        ImGui::TextUnformatted(EditorLocalization::Text(
            "Ctrl+S Save | Ctrl+Shift+S Save As | Ctrl+O Open | Ctrl+N New\nCtrl+Z Undo | Ctrl+Y / Ctrl+Shift+Z Redo | "
            "Ctrl+D Duplicate\nDelete Delete tree | F Frame selected | Escape Deselect\nT Move | R Rotate | "
            "S Scale (viewport)\nAlt+LMB Orbit | MMB Pan | RMB Zoom | Shift Fly\nDrag objects to change parent; drop "
            "on Scene root to detach.\nDrag assets to viewport or compatible Inspector fields.\nPlay uses an isolated "
            "scene snapshot; Stop restores edit state."));
        ImGui::End();
    }
    ImGui::Begin(EditorLocalization::Label("Scene"));
    ImGui::Text("%s%s", s.GetName().c_str(), mDocument.Dirty() ? " *" : "");
    ImGui::Text("%s", mScenePath);
    ImGui::Text(EditorLocalization::Text("Controller: %s"), Input::GetInstance()->GetGamepadBackendName());
    ImGui::Text(EditorLocalization::Text("%.1f FPS | %.2f ms | %zu history states"), ImGui::GetIO().Framerate,
                1000.f / std::max(1.f, ImGui::GetIO().Framerate), mDocument.Size());
    ImGui::BeginDisabled(mPlaying);
    std::string sceneName = s.GetName();
    if (Text("Scene name", sceneName) && !sceneName.empty())
        s.SetName(sceneName);
    ImGui::TextUnformatted(EditorLocalization::Text("Prefab path"));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##Prefab path", mPrefabPath, sizeof(mPrefabPath));
    if (ImGui::Button(EditorLocalization::Label("Save selected prefab")))
        Run([&] { SceneSerializer::SavePrefab(w, mSelected, UTF8ToWString(mPrefabPath)); });
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Instantiate prefab")))
        Run([&] { Select(w, SceneSerializer::InstantiatePrefab(w, UTF8ToWString(mPrefabPath))); });
    if (ImGui::Button(EditorLocalization::Label("Refresh prefab instances")))
        Run([&] {
            auto id = EditorActions::Id(w, mSelected);
            SceneSerializer::RefreshPrefabs(w);
            Select(w, EditorActions::Find(w, id));
        });
    if (w.IsValid(mSelected) && w.HasComponent<PrefabInstanceComponent>(mSelected))
    {
        auto overrides = SceneSerializer::PrefabOverrides(w, mSelected);
        ImGui::Text(EditorLocalization::Text("Overrides on selected object: %zu"), overrides.size());
        if (!overrides.empty() && ImGui::TreeNode(EditorLocalization::Label("Override differences")))
        {
            for (const auto &change : overrides)
                ImGui::TextWrapped("%s", change.c_str());
            ImGui::TreePop();
        }
        if (ImGui::Button(EditorLocalization::Label("Apply instance to prefab")))
            Run([&] {
                auto id = EditorActions::Id(w, mSelected);
                SceneSerializer::ApplyPrefab(w, mSelected);
                Select(w, EditorActions::Find(w, id));
            });
        ImGui::SameLine();
        if (ImGui::Button(EditorLocalization::Label("Revert prefab instance")))
            Run([&] {
                auto id = EditorActions::Id(w, mSelected);
                SceneSerializer::RevertPrefab(w, mSelected);
                Select(w, EditorActions::Find(w, id));
            });
    }
    if (ImGui::CollapsingHeader(EditorLocalization::Label("Environment")))
    {
        auto &settings = w.settings;
        AssetPath("Sky texture", settings.environment, "|.dds|");
        AssetPath("Diffuse IBL", settings.diffuseIBL, "|.dds|");
        AssetPath("Specular IBL", settings.specularIBL, "|.dds|");
        AssetPath("BRDF LUT", settings.brdfLut, "|.dds|");
        ImGui::Checkbox(EditorLocalization::Label("Draw sky"), &settings.skyEnabled);
        ImGui::DragFloat(EditorLocalization::Label("IBL intensity"), &settings.iblIntensity, .01f, 0, 20, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat(EditorLocalization::Label("IBL mip bias"), &settings.iblBias, .05f, 0, 10, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(EditorLocalization::Label("Auto exposure"), &settings.autoExposure);
        ImGui::BeginDisabled(settings.autoExposure);
        ImGui::DragFloat(EditorLocalization::Label("Exposure"), &settings.exposure, .01f, .01f, 20, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::EndDisabled();
        ImGui::Checkbox(EditorLocalization::Label("FXAA"), &settings.fxaa);
    }
    ImGui::EndDisabled();
    if (!mRuntimeError.empty())
        ImGui::TextWrapped("%s", mRuntimeError.c_str());
    if (!mError.empty())
    {
        ImGui::TextColored(EditorTheme::Color(231, 111, 111), "%s", mError.c_str());
        if (ImGui::SmallButton(EditorLocalization::Label("Dismiss")))
            mError.clear();
    }
    ImGui::End();
    if (ImGui::IsAnyItemActive() || ImGuizmo::IsUsing() || ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1) ||
        ImGui::GetIO().KeyCtrl || ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
        ImGui::IsKeyPressed(ImGuiKey_Space))
        mHistoryPending = true;
    if (!mPlaying && mHistoryPending && !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing())
    {
        Capture(s);
        mHistoryPending = false;
    }
    ConfirmPending(s);
    mTextInput = ImGui::GetIO().WantTextInput;
}
void EditorLayer::Hierarchy(World &w)
{
    ImGui::Begin(EditorLocalization::Label("Hierarchy"));
    ImGui::BeginDisabled(mPlaying);
    if (ImGui::Button(EditorLocalization::Label("Create")))
        ImGui::OpenPopup(EditorLocalization::Label("Create object"));
    if (ImGui::BeginPopup(EditorLocalization::Label("Create object")))
    {
        const char *names[] = {"Empty", "Plane", "Cube", "Sphere", "Quad"};
        for (int i = 0; i < 5; ++i)
            if (ImGui::MenuItem(EditorLocalization::Label(names[i])))
                Run([&] { Create(w, BuiltinMesh(i)); });
        if (ImGui::MenuItem(EditorLocalization::Label("Camera")))
            Run([&] {
                Create(w, BuiltinMesh::None);
                w.SetName(mSelected, "Camera");
                CameraComponent c;
                c.primary = w.View<CameraComponent>().begin() == w.View<CameraComponent>().end();
                w.AddComponent<CameraComponent>(mSelected, c);
            });
        if (ImGui::MenuItem(EditorLocalization::Label("Sun")))
            Run([&] {
                auto lights = w.View<DirectionalLightComponent>();
                if (lights.begin() != lights.end())
                    Select(w, *lights.begin());
                else
                {
                    Create(w, BuiltinMesh::None);
                    w.SetName(mSelected, "Sun");
                    w.AddComponent<DirectionalLightComponent>(mSelected);
                    Camera basis;
                    basis.SetLookDirection(Vector3(.35f, -1, .25f).NormalizedCopy());
                    w.GetComponent<TransformComponent>(mSelected).rotation = basis.GetRotation();
                }
            });
        if (ImGui::MenuItem(EditorLocalization::Label("UI Text")))
            Run([&] {
                Create(w, BuiltinMesh::None);
                w.AddComponent<UiElementComponent>(mSelected);
            });
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Duplicate")))
        Run([&] { Duplicate(w); });
    ImGui::SameLine();
    if (ImGui::Button(EditorLocalization::Label("Delete")))
        Run([&] {
            EditorActions::DeleteTree(w, mSelected);
            Select(w, entt::null);
        });
    ImGui::EndDisabled();
    static ImGuiTextFilter search;
    ImGui::Separator();
    ImGui::TextDisabled(EditorLocalization::Text("WORLD OUTLINER"));
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##Search objects", EditorLocalization::Text("Search objects..."), search.InputBuf,
                                 sizeof(search.InputBuf)))
        search.Build();
    std::vector<Entity> roots;
    std::map<Entity, std::vector<Entity>> children;
    for (auto e : w.View<PersistentIdComponent>())
    {
        auto h = w.TryGet<HierarchyComponent>(e);
        auto parent = h ? EditorActions::Find(w, h->parentId) : entt::null;
        if (parent == entt::null)
            roots.push_back(e);
        else
            children[parent].push_back(e);
    }
    auto label = [&](Entity e) {
        auto n = w.TryGet<NameComponent>(e);
        return n ? n->value : "Object";
    };
    auto sort = [&](std::vector<Entity> &entities) {
        std::sort(entities.begin(), entities.end(), [&](Entity a, Entity b) {
            auto kind = [&](Entity entity) {
                return w.HasComponent<CameraComponent>(entity)             ? 0
                       : w.HasComponent<DirectionalLightComponent>(entity) ? 1
                                                                           : 2;
            };
            if (kind(a) != kind(b))
                return kind(a) < kind(b);
            auto x = label(a), y = label(b);
            return x == y ? EditorActions::Id(w, a) < EditorActions::Id(w, b) : x < y;
        });
    };
    ImGui::TextDisabled(EditorLocalization::Text("%zu root objects"), roots.size());
    ImGui::Spacing();
    sort(roots);
    for (auto &[parent, list] : children)
        sort(list);
    ImGui::Selectable(EditorLocalization::Label("Scene root (drop to detach)"), false);
    if (!mPlaying && ImGui::BeginDragDropTarget())
    {
        if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ENTITY"))
        {
            auto entity = *static_cast<const Entity *>(payload->Data);
            Run([&] { EditorActions::Reparent(w, entity, entt::null); });
        }
        ImGui::EndDragDropTarget();
    }
    std::function<bool(Entity)> matches = [&](Entity e) {
        if (search.PassFilter(label(e).c_str()))
            return true;
        for (auto c : children[e])
            if (matches(c))
                return true;
        return false;
    };
    std::function<void(Entity)> draw = [&](Entity e) {
        if (!w.IsValid(e) || !matches(e))
            return;
        auto flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (e == mSelected)
            flags |= ImGuiTreeNodeFlags_Selected;
        if (children[e].empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (search.IsActive())
            ImGui::SetNextItemOpen(true);
        bool open =
            ImGui::TreeNodeEx(reinterpret_cast<void *>(uintptr_t(uint32_t(e)) + 1), flags, "%s", label(e).c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            Select(w, e);
        if (!mPlaying && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("ATOM_ENTITY", &e, sizeof(e));
            ImGui::TextUnformatted(label(e).c_str());
            ImGui::EndDragDropSource();
        }
        if (!mPlaying && ImGui::BeginDragDropTarget())
        {
            if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ENTITY"))
            {
                auto entity = *static_cast<const Entity *>(payload->Data);
                Run([&] { EditorActions::Reparent(w, entity, e); });
            }
            if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
            {
                auto path = std::filesystem::path(UTF8ToWString(static_cast<const char *>(payload->Data)));
                if (path.extension() == L".mat")
                    Run([&] { ApplyMaterial(w, e, path); });
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem())
        {
            Select(w, e);
            ImGui::BeginDisabled(mPlaying);
            if (ImGui::MenuItem(EditorLocalization::Label("Duplicate subtree")))
                Run([&] { Duplicate(w); });
            if (ImGui::MenuItem(EditorLocalization::Label("Delete subtree")))
                Run([&] {
                    EditorActions::DeleteTree(w, e);
                    Select(w, entt::null);
                });
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
        if (open)
        {
            for (auto child : children[e])
                draw(child);
            ImGui::TreePop();
        }
    };
    for (auto e : roots)
        draw(e);
    ImGui::End();
}
void EditorLayer::Inspector(Scene &s)
{
    auto &w = s.GetWorld();
    ImGui::Begin(EditorLocalization::Label("Inspector"));
    if (!w.IsValid(mSelected))
    {
        ImGui::Spacing();
        ImGui::TextDisabled(EditorLocalization::Text("NO OBJECT SELECTED"));
        ImGui::Separator();
        ImGui::TextWrapped(
            EditorLocalization::Text("Select an object in the viewport or Hierarchy to edit its properties."));
        ImGui::End();
        return;
    }
    auto e = mSelected;
    ImGui::BeginDisabled(mPlaying);
    auto name = w.TryGet<NameComponent>(e);
    std::string objectName = name ? name->value : "Object";
    ImGui::TextUnformatted(objectName.c_str());
    ImGui::TextDisabled("%s",
                        EditorLocalization::Text(w.HasComponent<CameraComponent>(e)             ? "CAMERA"
                                                 : w.HasComponent<DirectionalLightComponent>(e) ? "DIRECTIONAL LIGHT"
                                                                                                : "SCENE OBJECT"));
    ImGui::Separator();
    if (Text("Name", objectName))
        w.SetName(e, objectName);
    ImGui::TextDisabled(EditorLocalization::Text("Persistent ID"));
    EditorTheme::Tooltip(w.GetComponent<PersistentIdComponent>(e).value.c_str());
    ImGui::Spacing();
    if (auto t = w.TryGet<TransformComponent>(e);
        t && ImGui::CollapsingHeader(EditorLocalization::Label("Transform"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        TransformEditor(*t);
        auto h = w.TryGet<HierarchyComponent>(e);
        std::string parent = h ? h->parentId : "";
        if (ImGui::BeginCombo(EditorLocalization::Label("Parent"),
                              parent.empty() ? EditorLocalization::Text("None") : parent.c_str()))
        {
            if (ImGui::Selectable(EditorLocalization::Label("None"), parent.empty()))
            {
                Run([&] { EditorActions::Reparent(w, e, entt::null); });
            }
            for (auto p : w.View<PersistentIdComponent, TransformComponent>())
                if (p != e)
                {
                    auto id = w.GetComponent<PersistentIdComponent>(p).value;
                    auto n = w.TryGet<NameComponent>(p);
                    ImGui::PushID(int(uint32_t(p)));
                    if (ImGui::Selectable(n ? n->value.c_str() : id.c_str(), parent == id))
                    {
                        Run([&] { EditorActions::Reparent(w, e, p); });
                    }
                    ImGui::PopID();
                }
            ImGui::EndCombo();
        }
    }
    SceneObjectInspector(s);
    if (auto source = w.TryGet<MeshSourceComponent>(e);
        source && ImGui::CollapsingHeader(EditorLocalization::Label("Mesh")))
    {
        int type = int(source->primitive);
        bool rebuild = EditorLocalization::Combo("Primitive", &type, "Model asset\0Plane\0Cube\0Sphere\0Quad\0");
        auto id = EditorActions::Id(w, e);
        if (mModelDraftId != id)
        {
            mModelDraft = source->asset;
            mModelDraftId = id;
        }
        AssetPath("Model path", mModelDraft, "|.obj|.fbx|.gltf|.glb|.dae|.ply|.stl|");
        rebuild |= ImGui::Button(EditorLocalization::Label("Apply mesh"));
        if (rebuild)
            Run([&] {
                auto previous = *source;
                if (BuiltinMesh(type) == BuiltinMesh::None && !AssetManager::LoadModel(mModelDraft))
                    throw std::runtime_error("Cannot load model: " + mModelDraft);
                source->primitive = BuiltinMesh(type);
                source->asset = mModelDraft;
                try
                {
                    SceneSerializer::RebuildMesh(w, e);
                }
                catch (...)
                {
                    *source = previous;
                    throw;
                }
            });
        if (ImGui::Checkbox(EditorLocalization::Label("Visible"), &source->visible))
        {
            if (auto mesh = w.TryGet<MeshComponent>(e))
                mesh->SetIsRender(source->visible);
        }
    }
    if (auto materials = w.TryGet<MaterialComponent>(e);
        materials && ImGui::CollapsingHeader(EditorLocalization::Label("Material"), ImGuiTreeNodeFlags_DefaultOpen))
        for (size_t i = 0; i < materials->mMaterials.size(); ++i)
        {
            ImGui::PushID(int(i));
            auto &m = materials->mMaterials[i];
            ImGui::Text(EditorLocalization::Text("Material %zu"), i);
            if (!mPlaying && ImGui::BeginDragDropTarget())
            {
                if (auto payload = ImGui::AcceptDragDropPayload("ATOM_ASSET"))
                {
                    auto path = std::filesystem::path(UTF8ToWString(static_cast<const char *>(payload->Data)));
                    if (path.extension() == L".mat")
                        Run([&] { ApplyMaterial(w, e, path, int(i)); });
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::SmallButton(EditorLocalization::Label("Preview this slot")))
            {
                mPreviewSlot = int(i);
                mPreviewFromSelection = true;
                mShowMaterials = true;
            }
            Color4("BaseColor", m.mBaseColor.ptr());
            Float("Metallic", &m.mMetallic, .01f, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            Float("Roughness", &m.mRoughness, .01f, .045f, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            Float3("Specular F0", m.mFresnelF0.ptr(), 0.01f, 0.0f, 1.0f);
            Color3("Emissive", m.mEmissive.ptr());
            Float("Normal strength", &m.mNormalScale, .01f, 0, 8);
            Float("AO", &m.mAO, .01f, 0, 1);
            const char *slots[] = {"BaseColor texture", "Metallic texture", "Roughness texture", "Normal texture",
                                   "AO texture",        "Emissive texture", "Specular texture"};
            for (size_t slot = 0; slot < 7; ++slot)
                AssetPath(slots[slot], m.texturePaths[slot], "|.dds|.png|.jpg|.jpeg|.bmp|.tga|.hdr|.exr|.tif|.tiff|");
            if (ImGui::TreeNode(EditorLocalization::Label("UV transform")))
            {
                TransformEditor(m.uvTransform);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    if (auto c = w.TryGet<ColliderComponent>(e);
        c && ImGui::CollapsingHeader(EditorLocalization::Label("Collider"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        int shape = int(c->shape);
        EditorLocalization::Combo("Shape", &shape, "Box\0Sphere\0Capsule\0Triangle Mesh (static)\0Convex Hull\0");
        c->shape = ColliderShape(shape);
        if (c->shape == ColliderShape::Mesh || c->shape == ColliderShape::ConvexHull)
        {
            ImGui::TextWrapped(EditorLocalization::Text(
                "Uses the Mesh bind pose. Triangle Mesh requires a static body; Convex Hull supports moving bodies."));
            if (c->shape == ColliderShape::Mesh)
                if (auto body = w.TryGet<RigidBodyComponent>(e))
                    body->motion = BodyMotion::Static;
        }
        Float3("Collider center", c->center.ptr(), .01f);
        Rotation("Collider rotation", c->rotation);
        if (c->shape == ColliderShape::Box)
            Float3("Half extents", c->halfExtents.ptr(), .01f, .001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        else
            Float("Radius", &c->radius, .01f, .001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (c->shape == ColliderShape::Capsule)
            Float("Half height", &c->halfHeight, .01f, .001f, 1000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(EditorLocalization::Label("Trigger"), &c->trigger);
        ImGui::Checkbox(EditorLocalization::Label("Visualize collider"), &c->visible);
    }
    if (auto r = w.TryGet<RigidBodyComponent>(e); r && ImGui::CollapsingHeader(EditorLocalization::Label("Rigid Body")))
    {
        int motion = int(r->motion);
        auto collider = w.TryGet<ColliderComponent>(e);
        bool staticMesh = collider && collider->shape == ColliderShape::Mesh;
        ImGui::BeginDisabled(staticMesh);
        EditorLocalization::Combo("Motion", &motion, "Static\0Kinematic\0Dynamic\0");
        ImGui::EndDisabled();
        r->motion = staticMesh ? BodyMotion::Static : BodyMotion(motion);
        Float("Mass", &r->mass, .1f, .001f, 10000, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        Float("Friction", &r->friction, .01f, 0, 2);
        Float("Restitution", &r->restitution, .01f, 0, 1);
        ImGui::Checkbox(EditorLocalization::Label("Gravity"), &r->gravity);
        Float3("Velocity", r->linearVelocity.ptr(), .05f);
    }
    if (auto joint = w.TryGet<JointComponent>(e);
        joint && ImGui::CollapsingHeader(EditorLocalization::Label("Joint"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(EditorLocalization::Label("Joint enabled"), &joint->enabled);
        int type = int(joint->type);
        EditorLocalization::Combo("Joint type", &type, "Fixed\0Distance\0Hinge\0");
        joint->type = JointType(type);
        Text("Connected body ID", joint->connectedBody);
        if (ImGui::BeginCombo(EditorLocalization::Label("Connect to"), joint->connectedBody.c_str()))
        {
            for (auto target : w.View<ColliderComponent, PersistentIdComponent>())
                if (target != e)
                {
                    auto name = w.TryGet<NameComponent>(target);
                    auto id = w.GetComponent<PersistentIdComponent>(target).value;
                    if (ImGui::Selectable((name ? name->value + "##" + id : id).c_str(), joint->connectedBody == id))
                        joint->connectedBody = id;
                }
            ImGui::EndCombo();
        }
        if (joint->type != JointType::Fixed)
        {
            Float3("Anchor", joint->anchor.ptr(), .01f);
            Float3("Connected anchor", joint->connectedAnchor.ptr(), .01f);
        }
        else
            ImGui::TextWrapped(EditorLocalization::Text("Welds the bodies in their current relative pose."));
        if (joint->type == JointType::Distance)
        {
            ImGui::DragFloat(EditorLocalization::Label("Min distance"), &joint->minDistance, .01f, 0,
                             joint->maxDistance, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(EditorLocalization::Label("Max distance"), &joint->maxDistance, .01f, joint->minDistance,
                             10000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        }
        if (joint->type == JointType::Hinge)
        {
            Float3("Hinge axis", joint->axis.ptr(), .01f);
            if (joint->axis.LengthSqr() < 1e-8f)
                joint->axis = Vector3::UP;
            joint->axis.Normalize();
            ImGui::DragFloat(EditorLocalization::Label("Min angle"), &joint->minAngle, .5f, -180, 0, "%.1f",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat(EditorLocalization::Label("Max angle"), &joint->maxAngle, .5f, 0, 180, "%.1f",
                             ImGuiSliderFlags_AlwaysClamp);
        }
    }
    if (auto scripts = w.TryGet<ScriptComponent>(e);
        scripts && ImGui::CollapsingHeader(EditorLocalization::Label("C# Scripts"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        size_t remove = size_t(-1);
        for (size_t i = 0; i < scripts->scripts.size(); ++i)
        {
            ImGui::PushID(int(i));
            auto &s = scripts->scripts[i];
            ImGui::Checkbox(EditorLocalization::Label("Enabled"), &s.enabled);
            AssetPath("Assembly", s.assembly, "|.dll|");
            Text("Class", s.type);
            std::string deleteField;
            for (auto &[key, value] : s.fields)
            {
                ImGui::PushID(key.c_str());
                Text(key.c_str(), value, 35);
                ImGui::SameLine();
                if (ImGui::SmallButton("X"))
                    deleteField = key;
                ImGui::PopID();
            }
            if (!deleteField.empty())
                s.fields.erase(deleteField);
            static char field[128] = "";
            static char value[256] = "";
            ImGui::InputText(EditorLocalization::Label("Field name"), field, sizeof(field));
            ImGui::InputText(EditorLocalization::Label("Field value"), value, sizeof(value));
            if (ImGui::Button(EditorLocalization::Label("Set field")) && field[0])
                s.fields[field] = value;
            ImGui::SameLine();
            if (ImGui::Button(EditorLocalization::Label("Remove script")))
                remove = i;
            ImGui::Separator();
            ImGui::PopID();
        }
        if (remove != size_t(-1))
            scripts->scripts.erase(scripts->scripts.begin() + remove);
        if (ImGui::Button(EditorLocalization::Label("Attach script")))
            scripts->scripts.emplace_back();
        ScriptCreation(w);
    }
    if (auto ui = w.TryGet<UiElementComponent>(e);
        ui && ImGui::CollapsingHeader(EditorLocalization::Label("UI"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        Text("Text (UTF-8)", ui->text);
        AssetPath("Image", ui->texture, "|.dds|.png|.jpg|.jpeg|.bmp|.tga|");
        Float2("Screen position", &ui->position.x, 1);
        Float2("Size", &ui->size.x, 1, 1, 4096, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        Float("Font size", &ui->fontSize, 1, 1, 256);
        Color4("UI color", ui->color.ptr());
        ImGui::Checkbox(EditorLocalization::Label("UI visible"), &ui->visible);
        ImGui::Checkbox(EditorLocalization::Label("Clickable"), &ui->interactable);
        ImGui::InputInt(EditorLocalization::Label("UI layer"), &ui->layer);
        Color4("Background", ui->background.ptr());
    }
    if (ImGui::Button(EditorLocalization::Label("Add component")))
        ImGui::OpenPopup(EditorLocalization::Label("Components"));
    if (ImGui::BeginPopup(EditorLocalization::Label("Components")))
    {
        if (!w.HasComponent<MeshSourceComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Mesh")))
        {
            Run([&] {
                w.AddComponent<MeshSourceComponent>(e, BuiltinMesh::Cube);
                SceneSerializer::RebuildMesh(w, e);
            });
        }
        if (!w.HasComponent<ColliderComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Collider")))
            w.AddComponent<ColliderComponent>(e);
        if (!w.HasComponent<RigidBodyComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Rigid Body")))
            w.AddComponent<RigidBodyComponent>(e);
        if (!w.HasComponent<JointComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Joint")))
            w.AddComponent<JointComponent>(e);
        if (!w.HasComponent<ScriptComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("C# Script")))
        {
            w.AddComponent<ScriptComponent>(e);
            w.GetComponent<ScriptComponent>(e).scripts.emplace_back();
        }
        if (!w.HasComponent<UiElementComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("UI")))
            w.AddComponent<UiElementComponent>(e);
        ImGui::EndPopup();
    }
    if (ImGui::Button(EditorLocalization::Label("Remove component")))
        ImGui::OpenPopup(EditorLocalization::Label("Remove component menu"));
    if (ImGui::BeginPopup(EditorLocalization::Label("Remove component menu")))
    {
        if (w.HasComponent<MeshSourceComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Mesh + materials")))
        {
            w.RemoveComponent<MeshComponent>(e);
            w.RemoveComponent<MaterialComponent>(e);
            w.RemoveComponent<MeshSourceComponent>(e);
        }
        if (w.HasComponent<ColliderComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Collider")))
            w.RemoveComponent<ColliderComponent>(e);
        if (w.HasComponent<RigidBodyComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Rigid body")))
            w.RemoveComponent<RigidBodyComponent>(e);
        if (w.HasComponent<ScriptComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Scripts")))
            w.RemoveComponent<ScriptComponent>(e);
        if (w.HasComponent<JointComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("Joint")))
            w.RemoveComponent<JointComponent>(e);
        if (w.HasComponent<UiElementComponent>(e) && ImGui::MenuItem(EditorLocalization::Label("UI")))
            w.RemoveComponent<UiElementComponent>(e);
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    ImGui::End();
}
} // namespace AtomEngine
