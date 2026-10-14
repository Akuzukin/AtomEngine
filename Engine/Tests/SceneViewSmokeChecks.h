#pragma once
#include "Runtime/Function/Scene/SceneView.h"
#include "Editor/CameraGizmos.h"
inline void RunSceneViewSmokeChecks()
{
    using namespace AtomEngine;
    World world;
    Camera view;
    view.SetLookAt(Vector3(4, 6, -10), Vector3::ZERO, Vector3::UP);
    view.Update();
    EnsureSceneViewObjects(world, view);
    auto count = world.View<PersistentIdComponent>().size();
    EnsureSceneViewObjects(world, view);
    FeatureCheck(world.View<PersistentIdComponent>().size() == count, "Scene defaults created duplicates");
    auto camera = SceneCamera(world);
    FeatureCheck(camera != entt::null, "No scene camera");
    world.GetComponent<CameraComponent>(camera).fieldOfView = 68;
    auto sun = *world.View<DirectionalLightComponent>().begin();
    world.GetComponent<DirectionalLightComponent>(sun).intensity = 2.75f;
    world.settings.skyMode=1;world.settings.skyIntensity=1.4f;world.settings.skyRotation=120;world.settings.skySunSize=2;world.settings.skySunIntensity=5;
    world.settings.skyZenith={.2f,.3f,.4f};world.settings.skyHorizon={.5f,.6f,.7f};world.settings.skyGround={.1f,.2f,.3f};
    world.settings.environment="Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KEnvHDR.dds";
    auto xml = SceneSerializer::ToXml(world, "View settings");
    World restored;
    SceneSerializer::FromXml(restored, xml);
    FeatureCheck(restored.settings.skyMode==1 && restored.settings.environment==world.settings.environment && restored.settings.skyIntensity==1.4f && restored.settings.skyRotation==120 && restored.settings.skySunSize==2 && restored.settings.skySunIntensity==5 && restored.settings.skyZenith==world.settings.skyZenith && restored.settings.skyHorizon==world.settings.skyHorizon && restored.settings.skyGround==world.settings.skyGround,"Sky settings roundtrip failed");
    auto legacy=xml;auto skyMode=legacy.find("skyMode=\"1\"");legacy.erase(skyMode,std::string("skyMode=\"1\"").size());World oldScene;SceneSerializer::FromXml(oldScene,legacy);
    FeatureCheck(oldScene.settings.skyMode==0,"Legacy scene did not default to procedural sky");
    auto invalidSky=xml;invalidSky.replace(invalidSky.find("skyMode=\"1\""),std::string("skyMode=\"1\"").size(),"skyMode=\"2\"");bool skyRejected=false;try{World bad;SceneSerializer::FromXml(bad,invalidSky);}catch(...){skyRejected=true;}FeatureCheck(skyRejected,"Invalid sky mode accepted");
    Camera runtime;
    ApplySceneCamera(restored, runtime);
    FeatureCheck(std::abs(runtime.GetFOV() * 180 / Math::PI - 68) < .01f &&
                     (runtime.GetPosition() - view.GetPosition()).Length() < .001f &&
                     runtime.GetForwardVec().Dot(view.GetForwardVec()) > .999f,
                 "Runtime did not apply scene camera");
    FeatureCheck(restored.GetComponent<DirectionalLightComponent>(*restored.View<DirectionalLightComponent>().begin())
                         .intensity == 2.75f,
                 "Light roundtrip lost settings");
    auto frustum = CameraGizmos::BuildFrustum(world.GetComponent<CameraComponent>(camera),
                                              world.GetComponent<TransformComponent>(camera).GetMatrix(), 16.f / 9);
    for (int i = 0; i < 8; ++i)
    {
        auto p = frustum.GetFrustumCorner(static_cast<Frustum::CornerID>(i));
        auto clip = Vector4(p.x, p.y, p.z, 1) * runtime.GetViewProjMatrix();
        FeatureCheck(std::abs(std::abs(clip.x / clip.w) - 1) < .01f && std::abs(std::abs(clip.y / clip.w) - 1) < .01f &&
                         std::abs(clip.z / clip.w - (i < 4 ? 1.f : 0.f)) < .01f,
                     "Camera gizmo does not match projection/clip planes");
    }
    World gizmoWorld;
    auto object = gizmoWorld.CreateGameObject("Gizmo camera");
    object->AddComponent<TransformComponent>();
    CameraComponent settings;
    settings.nearClip = .1f;
    settings.farClip = 6;
    object->AddComponent<CameraComponent>(settings);
    Camera observer;
    observer.SetLookAt(Vector3(8, 6, -10), Vector3(0, 0, 2), Vector3::UP);
    observer.Update();
    auto &context = GraphicsContext::Begin(L"Camera gizmo regression");
    context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
    context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
    context.ClearColor(gSceneColorBuffer);
    context.ClearDepth(gSceneDepthBuffer);
    context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ, true);
    context.SetRenderTarget(gSceneColorBuffer.GetRTV(), gSceneDepthBuffer.GetDSV_DepthReadOnly());
    context.SetViewportAndScissor(0, 0, gSceneColorBuffer.GetWidth(), gSceneColorBuffer.GetHeight());
    CameraGizmos::Draw(gizmoWorld, observer);
    DebugDraw::Render(context);
    context.Finish(true);
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto capture = std::filesystem::path(exe).parent_path() / "Smoke/camera-frustum.raw";
    gSceneColorBuffer.ExportToFile(capture.wstring());
    std::ifstream image(capture, std::ios::binary);
    uint32_t header[4]{};
    image.read(reinterpret_cast<char *>(header), 16);
    FeatureCheck(header[0] == DXGI_FORMAT_R10G10B10A2_UNORM, "Unexpected gizmo capture format");
    std::vector<uint32_t> pixels(size_t(header[1] / 4) * header[3]);
    image.read(reinterpret_cast<char *>(pixels.data()), pixels.size() * 4);
    size_t visible = 0;
    for (auto pixel : pixels)
        visible += (pixel & 1023) < 100 && ((pixel >> 10) & 1023) > 500 && ((pixel >> 20) & 1023) > 500;
    FeatureCheck(visible > 100, "Camera gizmo did not render visible line pixels");
    EditorDocument history;
    history.Reset(restored, "View settings");
    auto c = SceneCamera(restored);
    auto id = EditorActions::Id(restored, c);
    restored.GetComponent<CameraComponent>(c).fieldOfView = 92;
    history.Capture(restored, "View settings", id, "Camera FOV");
    history.Undo(restored);
    FeatureCheck(restored.GetComponent<CameraComponent>(SceneCamera(restored)).fieldOfView == 68, "Camera undo failed");
    history.Redo(restored);
    FeatureCheck(restored.GetComponent<CameraComponent>(SceneCamera(restored)).fieldOfView == 92, "Camera redo failed");
    auto before = SceneSerializer::ToXml(restored, "View settings");
    auto bad = before;
    auto offset = bad.find("fieldOfView=\"92\"");
    FeatureCheck(offset != std::string::npos, "Missing camera XML");
    bad.replace(offset, 16, "fieldOfView=\"0\"");
    bool rejected = false;
    try
    {
        SceneSerializer::FromXml(restored, bad);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    FeatureCheck(rejected && before == SceneSerializer::ToXml(restored, "View settings"),
                 "Invalid camera load changed scene");
    WriteLog(
        LogLevel::Info, "Smoke",
        "Scene camera projection/pose, sun XML, undo/redo, invalid camera rollback and frustum geometry/pixels passed");
}
