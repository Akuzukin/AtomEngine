#pragma once
#include "Runtime/Resource/Primitive.h"
#include "Runtime/Function/Camera/DebugCamera.h"
#include "Runtime/Function/Input/InputBackend.h"
#include "Runtime/Function/Render/SkyboxRenderer.h"
#include "DirectXTex.h"
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
    auto procedural = render(false);
    std::filesystem::copy_file(path,path.parent_path()/"sky-procedural.raw",std::filesystem::copy_options::overwrite_existing);
    auto different = [](const auto &a, const auto &b) { size_t count = 0; for (size_t i = 0; i < a.size(); ++i) count += a[i] != b[i]; return count; };
    SceneSettings skySettings;
    FeatureCheck(skySettings.skyMode == 0 && skySettings.environment.empty(), "New scene does not default to procedural sky");
    skySettings.skyZenith=skySettings.skyHorizon=skySettings.skyGround=Vector3(0,.4f,0);skySettings.skySunIntensity=0;
    sky.SetSkySettings(skySettings);auto green=render(false);
    FeatureCheck(different(procedural,green)>green.size()/2,"Procedural sky colors did not update");
    skySettings=SceneSettings{};skySettings.skyMode=1;sky.SetSkySettings(skySettings);
    sky.SetEnvironmentMap(L"Asset/Textures/Environment/NightSkyHDRI004_4K/NightSkyHDRI004_4KEnvHDR.dds");
    FeatureCheck(sky.TextureError().empty(),"Valid cube sky rejected");auto visible = render(false);
    std::filesystem::copy_file(path,path.parent_path()/"sky-texture.raw",std::filesystem::copy_options::overwrite_existing);
    auto blocked = render(true);
    size_t changed = 0;
    for (size_t i = 0; i < visible.size(); ++i)
        changed += visible[i] != blocked[i];
    FeatureCheck(changed > visible.size() / 2, "Skybox was culled or failed its reverse-Z depth test");
    sky.SetEnvironmentMap(L"Asset/Textures/Environment/PaperMill/PaperMill_E_3kEnvHDR.dds");auto replacement=render(false);
    FeatureCheck(different(visible,replacement)>visible.size()/4,"Replacing cubemap did not update sky pixels");
    skySettings.skyRotation=90;sky.SetSkySettings(skySettings);auto rotated=render(false);
    FeatureCheck(different(replacement,rotated)>visible.size()/4,"Sky texture rotation did not update pixels");
    // 真实二维全景 DDS 和 PNG，验证 descriptor 类型、转换和经纬度采样。
    DirectX::ScratchImage panorama;ThrowIfFailed(panorama.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,64,32,1,1));
    auto image=panorama.GetImage(0,0,0);
    for(size_t y=0;y<32;++y)for(size_t x=0;x<64;++x){auto pixel=image->pixels+y*image->rowPitch+x*4;pixel[0]=uint8_t(x*4);pixel[1]=uint8_t(y*8);pixel[2]=160;pixel[3]=255;}
    auto panoramaFile=path.parent_path()/"sky-panorama.dds";
    ThrowIfFailed(DirectX::SaveToDDSFile(panorama.GetImages(),panorama.GetImageCount(),panorama.GetMetadata(),DirectX::DDS_FLAGS_NONE,panoramaFile.c_str()));
    sky.SetEnvironmentMap(panoramaFile.wstring());FeatureCheck(sky.TextureError().empty(),"2D panorama DDS rejected");auto panoramaPixels=render(false);
    std::filesystem::copy_file(path,path.parent_path()/"sky-panorama.raw",std::filesystem::copy_options::overwrite_existing);
    FeatureCheck(different(rotated,panoramaPixels)>visible.size()/2,"Panorama sky failed to draw");
    auto pngFile=path.parent_path()/"sky-panorama.png";ThrowIfFailed(DirectX::SaveToWICFile(*image,DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),pngFile.c_str()));
    sky.SetEnvironmentMap(pngFile.wstring());FeatureCheck(sky.TextureError().empty(),"PNG panorama conversion failed");render(false);
    sky.SetEnvironmentMap(L"missing-sky.dds");FeatureCheck(!sky.TextureError().empty(),"Missing texture has no diagnostic");auto fallback=render(false);
    FeatureCheck(different(fallback,procedural)==0,"Missing texture did not fall back to procedural sky");
    DirectX::ScratchImage array;ThrowIfFailed(array.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,4,4,2,1));auto arrayFile=path.parent_path()/"sky-array.dds";
    ThrowIfFailed(DirectX::SaveToDDSFile(array.GetImages(),array.GetImageCount(),array.GetMetadata(),DirectX::DDS_FLAGS_NONE,arrayFile.c_str()));
    sky.SetEnvironmentMap(arrayFile.wstring());FeatureCheck(!sky.TextureError().empty(),"Texture array incorrectly accepted as a sky cubemap");
    FeatureCheck(different(render(false),procedural)==0,"Unsupported sky descriptor did not safely fall back");
    auto updateLighting=[&]{auto &context=GraphicsContext::Begin(L"Sky lighting cache regression");sky.UpdateEnvironmentLighting(context);context.Finish(true);};
    updateLighting();auto generation=sky.LightingGeneration();FeatureCheck(generation==1,"Procedural sky IBL not generated");
    updateLighting();FeatureCheck(sky.LightingGeneration()==generation,"Unchanged sky regenerated IBL");
    skySettings.skyIntensity=.7f;sky.SetSkySettings(skySettings);updateLighting();FeatureCheck(sky.LightingGeneration()==generation+1,"Sky brightness failed to invalidate lighting cache");
    sky.SetSkySettings(skySettings,Vector3::UP);updateLighting();FeatureCheck(sky.LightingGeneration()==generation+2,"Sun direction failed to invalidate lighting cache");
    sky.SetEnvironmentMap(panoramaFile.wstring());updateLighting();FeatureCheck(sky.LightingGeneration()==generation+2,"Texture mode regenerated procedural IBL");
    sky.Shutdown();
    occluder.Destroy();
    WriteLog(LogLevel::Info, "Smoke",
             "Procedural sky, cube replacement/rotation, DDS/PNG panoramas, invalid texture fallback, depth occlusion and IBL cache passed");
}
