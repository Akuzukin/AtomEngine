#pragma once
#include "Runtime/Resource/Primitive.h"
#include "Runtime/Function/Camera/DebugCamera.h"
#include "Runtime/Function/Input/InputBackend.h"
#include "Runtime/Function/Render/SkyboxRenderer.h"
inline void RunRenderCameraSmokeChecks()
{
    using namespace AtomEngine;
    auto cube = Primitive::Generate(BuiltinMesh::Cube);
    FeatureCheck(cube.vertices.size() == 24 && cube.indices.size() == 36, "Cube geometry counts changed");
    for (size_t i = 0; i < cube.indices.size(); i += 3)
    {
        auto &a = cube.vertices.at(cube.indices[i]);
        auto &b = cube.vertices.at(cube.indices[i + 1]);
        auto &c = cube.vertices.at(cube.indices[i + 2]);
        FeatureCheck((b.position - a.position).Cross(c.position - a.position).Dot(a.normal) > .99f,
                     "Cube triangle faces inward");
        FeatureCheck(std::abs(a.normal.Dot(a.tangent)) < 1e-5f && std::abs(a.normal.Dot(a.bitangent)) < 1e-5f,
                     "Cube tangent basis is invalid");
    }
    InputSnapshot modifiers;
    modifiers.keys[0xA5] = true;
    modifiers.keys[0xA1] = true;
    modifiers.NormalizeModifiers();
    FeatureCheck(modifiers.keys[0x12] && modifiers.keys[0x10], "Right Alt/Shift did not normalize");
    Camera camera;
    Vector3 target(3, 1, 5);
    camera.SetLookAt(Vector3(3, 2, -5), target, Vector3::UP);
    camera.Update();
    DebugCamera controller(camera);
    auto position = camera.GetPosition(), forward = camera.GetForwardVec();
    controller.Update(.2f, {});
    FeatureCheck((camera.GetPosition() - position).Length() < .001f && camera.GetForwardVec().Dot(forward) > .999f,
                 "Idle controller changed the camera");
    controller.SetTarget(target);
    auto radius = controller.GetDistance();
    DebugCameraInput controls;
    controls.orbit = true;
    controls.dx = 80;
    controls.dy = 30;
    controller.Update(.2f, controls);
    FeatureCheck(std::abs((camera.GetPosition() - target).Length() - radius) < .001f &&
                     camera.GetForwardVec().Dot((target - camera.GetPosition()).NormalizedCopy()) > .999f,
                 "Orbit did not preserve radius and target");
    forward = camera.GetForwardVec();
    controls = {};
    controls.fly = true;
    controller.Update(.2f, controls);
    FeatureCheck(camera.GetForwardVec().Dot(forward) > .999f, "Entering fly mode flipped the camera");
    auto right = camera.GetRightVec();
    controls.look = true;
    controls.dx = 60;
    controls.dy = 0;
    controller.Update(.2f, controls);
    FeatureCheck(camera.GetForwardVec().Dot(right) > .05f, "Fly mouse look did not turn right");
    forward = camera.GetForwardVec();
    controller.Update(.2f, {});
    FeatureCheck(camera.GetForwardVec().Dot(forward) > .999f, "Leaving fly mode flipped the camera");
    controls = {};
    controls.wheel = 1;
    controller.Update(.2f, controls);
    FeatureCheck(controller.GetDistance() < radius, "Wheel did not zoom the camera");
    radius = controller.GetDistance();
    controller.Update(.2f, {});
    FeatureCheck(std::abs(controller.GetDistance() - radius) < .001f, "Idle wheel input repeated zoom");

    // Render the actual sky shader into the scene target and inspect pixels.
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto path = std::filesystem::path(exe).parent_path() / "Smoke/skybox.raw";
    SkyboxRenderer sky;
    sky.Initialize();
    sky.SetEnvironmentMap(L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KEnvHDR.dds");
    DepthBuffer occluder(1.f);
    occluder.Create(L"Sky regression occlusion", gSceneDepthBuffer.GetWidth(), gSceneDepthBuffer.GetHeight(),
                    gSceneDepthBuffer.GetFormat());
    auto render = [&](bool occluded) {
        auto &context = GraphicsContext::Begin(L"Skybox regression");
        context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
        context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
        context.ClearColor(gSceneColorBuffer);
        context.ClearDepth(gSceneDepthBuffer);
        if (occluded)
        {
            context.TransitionResource(occluder, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            context.ClearDepth(occluder);
            context.CopyBuffer(gSceneDepthBuffer, occluder);
        }
        D3D12_VIEWPORT viewport{0, 0, float(gSceneColorBuffer.GetWidth()), float(gSceneColorBuffer.GetHeight()), 0, 1};
        D3D12_RECT scissor{0, 0, LONG(gSceneColorBuffer.GetWidth()), LONG(gSceneColorBuffer.GetHeight())};
        sky.Render(context, &camera, viewport, scissor);
        context.Finish(true);
        auto output = occluded ? path.parent_path() / "skybox-occluded.raw" : path;
        gSceneColorBuffer.ExportToFile(output.wstring());
        std::ifstream file(output, std::ios::binary);
        uint32_t header[4]{};
        file.read(reinterpret_cast<char *>(header), 16);
        FeatureCheck(header[0] == DXGI_FORMAT_R10G10B10A2_UNORM, "Unexpected sky capture format");
        std::vector<uint32_t> pixels(size_t(header[1] / 4) * header[3]);
        file.read(reinterpret_cast<char *>(pixels.data()), pixels.size() * 4);
        return pixels;
    };
    auto visible = render(false);
    auto blocked = render(true);
    size_t changed = 0;
    for (size_t i = 0; i < visible.size(); ++i)
        changed += visible[i] != blocked[i];
    FeatureCheck(changed > visible.size() / 2, "Skybox was culled or failed its reverse-Z depth test");
    sky.Shutdown();
    occluder.Destroy();
    WriteLog(LogLevel::Info, "Smoke",
             "Cube winding, sky pixels/depth, modifiers and camera orbit/fly/zoom checks passed");
}
