#include "EditorLayer.h"
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
    if (camera && ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Camera enabled", &camera->enabled);
        ImGui::Text("%s", camera->primary ? "Primary scene camera" : "Additional camera");
        if (!camera->primary && ImGui::Button("Make primary camera"))
        {
            for (auto e : world.View<CameraComponent>())
                world.GetComponent<CameraComponent>(e).primary = e == mSelected;
            mHistoryPending = true;
        }
        ImGui::DragFloat("Field of view", &camera->fieldOfView, .2f, 1, 179, "%.1f deg", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Near clip", &camera->nearClip, .01f, .001f, std::max(.001f, camera->farClip - .01f), "%.3f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Far clip", &camera->farClip, 1, camera->nearClip + .01f, 100000, "%.1f",
                         ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::Button("Align camera to editor view"))
            Run([&] {
                auto &view = scene.GetCamera();
                Transform pose(view.GetPosition(), view.GetRotation(), Vector3::UNIT_SCALE);
                EditorActions::SetWorldMatrix(world, mSelected, pose.GetMatrix());
                camera->fieldOfView = view.GetFOV() * 180.f / Math::PI;
                camera->nearClip = view.GetNearClip();
                camera->farClip = view.GetFarClip();
                mHistoryPending = true;
            });
        if (ImGui::Button("Look through camera"))
            Run([&] {
                ApplySceneCamera(world, scene.GetCamera(), mSelected);
                mFocus = scene.GetCamera().GetPosition() + scene.GetCamera().GetForwardVec() * 5.f;
            });
    }
    if (light && ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Sun enabled", &light->enabled);
        ImGui::ColorEdit3("Sun color", light->color.ptr());
        ImGui::DragFloat("Intensity", &light->intensity, .05f, 0, 1000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox("Cast shadows", &light->shadows);
        ImGui::Checkbox("Auto fit shadow", &light->autoFitShadow);
        auto direction = world.GetComponent<TransformComponent>(mSelected).GetMatrix().GetRotation() * Vector3::FORWARD;
        ImGui::Text("Direction: %.3f, %.3f, %.3f", direction.x, direction.y, direction.z);
        ImGui::TextWrapped(
            "Use Transform rotation or the Rotate tool to aim the sun. Position does not affect directional lighting.");
    }
}
void EditorLayer::PlayToolbar(Scene &scene)
{
    auto button = [&](const char *label, bool active, ImVec4 color) {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x + .08f, color.y + .08f, color.z + .08f, 1));
        }
        bool clicked = ImGui::Button(label, ImVec2(78, 0));
        if (active)
            ImGui::PopStyleColor(2);
        return clicked;
    };
    if (button(mPlaying ? "Stop" : "Play", mPlaying, ImVec4(.15f, .45f, .24f, 1)))
        Run([&] { TogglePlay(scene); });
    ImGui::SameLine();
    ImGui::BeginDisabled(!mPlaying);
    if (button(mPaused ? "Resume" : "Pause", mPaused, ImVec4(.55f, .37f, .1f, 1)))
        TogglePause();
    ImGui::SameLine();
    ImGui::BeginDisabled(!mPaused);
    if (ImGui::Button("Step"))
        StepFrame();
    ImGui::EndDisabled();
    ImGui::EndDisabled();
#ifndef RELEASE
    ImGui::SameLine();
    ImGui::Checkbox("Maximize on Play", &mMaximizeOnPlay);
#endif
    ImGui::Separator();
}
void EditorLayer::PlayViewport(Scene &scene)
{
    auto viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::Begin("Play view", nullptr,
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
        ImGui::Image((ImTextureID)mViewportTexture, size);
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
