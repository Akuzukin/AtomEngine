#include "EditorLayer.h"
#include "ScriptTools.h"
#include "ImGuiCommon.h"
#include "EditorTheme.h"
#include "EditorLocalization.h"
#include "CameraGizmos.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "imgui.h"
namespace AtomEngine
{
void EditorLayer::RenderCameraGizmos(Scene &scene)
{
    if (mShowCameraGizmos && !mPlaying)
        CameraGizmos::Draw(scene.GetWorld(), scene.GetCamera(), mSelected);
}
void EditorLayer::SceneObjectInspector(Scene &scene)
{
    auto &world = scene.GetWorld();
    auto camera = world.TryGet<CameraComponent>(mSelected);
    auto light = world.TryGet<DirectionalLightComponent>(mSelected);
    if (!camera && !light)
        return;
    if (camera && ImGui::CollapsingHeader(EditorLocalization::Label("Camera"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(EditorLocalization::Label("Camera enabled"), &camera->enabled);
        ImGui::Text("%s", EditorLocalization::Text(camera->primary ? "Primary scene camera" : "Additional camera"));
        if (!camera->primary && ImGui::Button(EditorLocalization::Label("Make primary camera")))
        {
            for (auto e : world.View<CameraComponent>())
                world.GetComponent<CameraComponent>(e).primary = e == mSelected;
            mHistoryPending = true;
        }
        ImGui::DragFloat(EditorLocalization::Label("Field of view"), &camera->fieldOfView, .2f, 1, 179, "%.1f deg",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat(EditorLocalization::Label("Near clip"), &camera->nearClip, .01f, .001f,
                         std::max(.001f, camera->farClip - .01f), "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat(EditorLocalization::Label("Far clip"), &camera->farClip, 1, camera->nearClip + .01f, 100000,
                         "%.1f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::Button(EditorLocalization::Label("Align camera to editor view")))
            Run([&] {
                auto &view = scene.GetCamera();
                Transform pose(view.GetPosition(), view.GetRotation(), Vector3::UNIT_SCALE);
                EditorActions::SetWorldMatrix(world, mSelected, pose.GetMatrix());
                camera->fieldOfView = view.GetFOV() * 180.f / Math::PI;
                camera->nearClip = view.GetNearClip();
                camera->farClip = view.GetFarClip();
                mHistoryPending = true;
            });
        if (ImGui::Button(EditorLocalization::Label("Look through camera")))
            Run([&] {
                ApplySceneCamera(world, scene.GetCamera(), mSelected);
                mFocus = scene.GetCamera().GetPosition() + scene.GetCamera().GetForwardVec() * 5.f;
            });
    }
    if (light &&
        ImGui::CollapsingHeader(EditorLocalization::Label("Directional Light"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(EditorLocalization::Label("Sun enabled"), &light->enabled);
        ImGui::ColorEdit3(EditorLocalization::Label("Sun color"), light->color.ptr());
        ImGui::DragFloat(EditorLocalization::Label("Intensity"), &light->intensity, .05f, 0, 1000, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(EditorLocalization::Label("Cast shadows"), &light->shadows);
        ImGui::Checkbox(EditorLocalization::Label("Auto fit shadow"), &light->autoFitShadow);
        auto direction = world.GetComponent<TransformComponent>(mSelected).GetMatrix().GetRotation() * Vector3::FORWARD;
        ImGui::Text(EditorLocalization::Text("Direction: %.3f, %.3f, %.3f"), direction.x, direction.y, direction.z);
        ImGui::TextWrapped(EditorLocalization::Text("Use Transform rotation or the Rotate tool to aim the sun. "
                                                    "Position does not affect directional lighting."));
    }
}
void EditorLayer::PlayToolbar(Scene &scene)
{
    auto hint = [](const char *key) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", EditorLocalization::Text(key));
    };
    if (EditorTheme::IconButton("###SimulationPlay", mPlaying ? EditorTheme::Icon::Stop : EditorTheme::Icon::Play,
                                mPlaying, EditorTheme::Color(46, 100, 64),
                                mPlaying ? EditorTheme::Gray(230) : EditorTheme::Color(112, 192, 138)))
        Run([&] { TogglePlay(scene); });
    hint(mPlaying ? "Stop simulation and restore the editing scene" : "Run the scene");
    ImGui::SameLine();
    ImGui::BeginDisabled(!mPlaying);
    if (EditorTheme::IconButton("###SimulationPause", mPaused ? EditorTheme::Icon::Play : EditorTheme::Icon::Pause,
                                mPaused, EditorTheme::Color(112, 84, 39)))
        TogglePause();
    hint(mPaused ? "Resume" : "Pause");
    ImGui::SameLine();
    ImGui::BeginDisabled(!mPaused);
    if (EditorTheme::IconButton("###SimulationStep", EditorTheme::Icon::Step, false, EditorTheme::Gray(51)))
        StepFrame();
    hint("Step");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mScriptBuild.valid());
    if (ImGui::Button(EditorLocalization::Label("Build scripts")))
        Run([&] { mScriptBuild = ScriptTools::Build(); });
    ImGui::EndDisabled();
#ifndef RELEASE
    ImGui::SameLine();
    ImGui::Checkbox(EditorLocalization::Label("Maximize on Play"), &mMaximizeOnPlay);
#endif
    ImGui::Separator();
}
void EditorLayer::PlayViewport(Scene &scene)
{
    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::Begin(EditorLocalization::Label("Play view"), nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoMove);
    PlayToolbar(scene);
    auto cursor = ImGui::GetCursorScreenPos(), size = ImGui::GetContentRegionAvail();
    mViewportHovered = false;
    mGizmoActive = false;
    if (mViewportTexture && size.x > 1 && size.y > 1)
    {
        // Keep rendering and runtime UI coordinates consistent with the expanded image.
        scene.GetCamera().SetAspectRatio(size.y / size.x);
        ImGuiCommon::ImageDisplayEncoded((ImTextureID)mViewportTexture, size);
        mViewportHovered = ImGui::IsItemHovered();
        auto mouse = ImGui::GetMousePos();
        mRuntimePointer = Vector2((mouse.x - cursor.x) * gSceneColorBuffer.GetWidth() / size.x,
                                  (mouse.y - cursor.y) * gSceneColorBuffer.GetHeight() / size.y);
    }
    ImGui::End();
}
void EditorLayer::ClearThumbnails()
{
    for (auto &[path, thumbnail] : mThumbnails)
        if (!thumbnail.handle.IsNull())
            Renderer::GetTextureHeap().Free(thumbnail.handle);
    mThumbnails.clear();
}
} // namespace AtomEngine
