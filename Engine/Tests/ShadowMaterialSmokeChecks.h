#pragma once
#include "Runtime/Resource/MaterialPreset.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "Editor/MaterialPreviewRenderer.h"
#include "Runtime/Function/Render/RenderPasses/ForwardRenderingPass.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"

inline void RunShadowMaterialSmokeChecks()
{
    using namespace AtomEngine;
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto folder = std::filesystem::path(exe).parent_path() / "Smoke";
    std::filesystem::create_directories(folder);
    for (auto name : {"Gold", "BrushedSteel", "RedPlastic", "BlueCeramic", "Rubber"})
        FeatureCheck(MaterialPreset::Load(std::filesystem::path("Asset/Materials") / (std::string(name) + ".mat"))
                         .material.overrideModelTextures,
                     "Preset not standalone");
    auto red = MaterialPreset::Load("Asset/Materials/RedPlastic.mat");
    red.material.uvTransform.scale = Vector3(2, 3, 1);
    red.material.packedTextureChannels = 3;
    red.Save(folder / "roundtrip.mat");
    auto copy = MaterialPreset::Load(folder / "roundtrip.mat");
    FeatureCheck(copy.material.mBaseColor.r == red.material.mBaseColor.r && copy.material.packedTextureChannels == 3 &&
                     copy.material.uvTransform.scale.y == 3,
                 "Material roundtrip lost data");
    World edits;
    auto object = edits.CreateGameObject("Material target");
    object->AddComponent<MaterialComponent>();
    object->GetComponent<MaterialComponent>().mMaterials.resize(2);
    auto id = EditorActions::Id(edits, *object);
    EditorDocument history;
    auto blue = MaterialPreset::Load("Asset/Materials/BlueCeramic.mat");
    blue.Apply(edits, *object, 1);
    FeatureCheck(object->GetComponent<MaterialComponent>().mMaterials[0].mBaseColor.r == 1 &&
                     object->GetComponent<MaterialComponent>().mMaterials[1].mBaseColor.b == blue.material.mBaseColor.b,
                 "Applying one slot changed other slots");
    object->GetComponent<MaterialComponent>().mMaterials.resize(1);
    history.Reset(edits, "Materials");
    blue.Apply(edits, *object, 0);
    history.Capture(edits, "Materials", id, "Apply material");
    history.Undo(edits);
    auto restored = EditorActions::Find(edits, id);
    FeatureCheck(edits.GetComponent<MaterialComponent>(restored).mMaterials[0].mBaseColor.r == 1,
                 "Undo did not restore material");
    history.Redo(edits);
    restored = EditorActions::Find(edits, id);
    blue.Apply(edits, restored);
    FeatureCheck(edits.GetComponent<MaterialComponent>(restored).mMaterials[0].overrideModelTextures,
                 "Apply all did not persist overrides");
    auto before = SceneSerializer::ToXml(edits, "Materials");
    auto missing = blue;
    missing.material.texturePaths[0] = "Asset/Textures/MissingPresetTexture.dds";
    bool missingRejected = false;
    try
    {
        missing.Apply(edits, restored);
    }
    catch (const std::exception &)
    {
        missingRejected = true;
    }
    FeatureCheck(missingRejected && before == SceneSerializer::ToXml(edits, "Materials"),
                 "Missing preset texture modified scene");
    bool rejected = false;
    try
    {
        blue.Apply(edits, restored, 9);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    FeatureCheck(rejected && before == SceneSerializer::ToXml(edits, "Materials"), "Invalid apply modified scene");

    auto readPixels = [&](ColorBuffer &buffer, const std::string &name) {
        auto path = folder / (name + ".raw");
        buffer.ExportToFile(path.wstring());
        std::ifstream file(path, std::ios::binary);
        uint32_t header[4]{};
        file.read(reinterpret_cast<char *>(header), 16);
        FeatureCheck(header[0] == DXGI_FORMAT_R10G10B10A2_UNORM, "Unexpected capture format");
        std::vector<uint32_t> pixels(size_t(header[1] / 4) * header[3]);
        file.read(reinterpret_cast<char *>(pixels.data()), pixels.size() * 4);
        return pixels;
    };
    {
        MaterialPreviewRenderer preview;
        preview.Initialize();
        preview.Render(MaterialPreset::Load("Asset/Materials/RedPlastic.mat").material);
        auto redPixels = readPixels(preview.Buffer(), "preview-red");
        auto inherited = blue.material;
        inherited.overrideModelTextures = false;
        preview.Render(inherited, 0, .25f, Primitive::GetModel(BuiltinMesh::Cube));
        auto inheritedPixels = readPixels(preview.Buffer(), "preview-inherited");
        FeatureCheck(!inheritedPixels.empty(), "Inherited preview failed");
        preview.Render(blue.material);
        auto bluePixels = readPixels(preview.Buffer(), "preview-blue");
        size_t different = 0;
        for (size_t i = 0; i < redPixels.size(); ++i)
            different += redPixels[i] != bluePixels[i];
        FeatureCheck(different > redPixels.size() / 8, "Preview sphere did not reflect material changes");
    }
    ShadowCamera shadow;
    shadow.UpdateMatrix(Vector3(0, -1, 0), Vector3(0, 0, 0), Vector3(10, 10, 10), 2048, 2048);
    auto caster = Vector4(0, 2, 0, 1) * shadow.GetShadowMatrix(),
         receiver = Vector4(0, 0, 0, 1) * shadow.GetShadowMatrix();
    FeatureCheck(caster.z > receiver.z && receiver.z > 0 && receiver.z < 1 && std::abs(receiver.x - .5f) < .002f &&
                     std::abs(receiver.y - .5f) < .002f,
                 "Shadow projection violates reverse-Z or texture coordinates");
    // Real lighting test: identical scene, with and without the shadow depth map.
    World world;
    auto add = [&](const char *name, BuiltinMesh shape, Vector3 position, Vector3 scale) {
        auto object = world.CreateGameObject(name);
        auto model = Primitive::GetModel(shape);
        object->AddComponent<TransformComponent>(position, Quaternion::IDENTITY, scale);
        object->AddComponent<MeshComponent>(model);
        object->AddComponent<MaterialComponent>(model);
        return Entity(*object);
    };
    add("Ground", BuiltinMesh::Plane, Vector3(0, 0, 0), Vector3(12, 1, 12));
    add("Caster", BuiltinMesh::Cube, Vector3(0, 1, 0), Vector3(2, 2, 2));
    Camera camera;
    camera.SetLookAt(Vector3(6, 5, -8), Vector3(0, .5f, 0), Vector3::UP);
    camera.Update();
    EnsureSceneViewObjects(world,camera);
    auto sun=*world.View<DirectionalLightComponent>().begin();
    ForwardRenderingPass pass;
    pass.SetWorld(world);
    pass.SetCamera(&camera);
    pass.SetViewportAndScissor({0, 0, float(gSceneColorBuffer.GetWidth()), float(gSceneColorBuffer.GetHeight()), 0, 1},
                               {0, 0, LONG(gSceneColorBuffer.GetWidth()), LONG(gSceneColorBuffer.GetHeight())});
    auto render = [&](bool enabled) {
        auto &context = GraphicsContext::Begin(L"Shadow regression");
        for (auto e : world.View<MeshComponent, TransformComponent, MaterialComponent>())
            world.GetComponent<MeshComponent>(e).Update(context, world.GetComponent<TransformComponent>(e),
                                                        world.GetComponent<MaterialComponent>(e), 0);
        world.GetComponent<DirectionalLightComponent>(sun).shadows=enabled;
        pass.Render(context);
        context.Finish(true);
        return readPixels(gSceneColorBuffer, enabled ? "shadow-on" : "shadow-off");
    };
    auto on = render(true), off = render(false);
    size_t darker = 0;
    for (size_t i = 0; i < on.size(); ++i)
    {
        auto sum = [](uint32_t p) { return (p & 1023) + ((p >> 10) & 1023) + ((p >> 20) & 1023); };
        darker += sum(on[i]) + 8 < sum(off[i]);
    }
    FeatureCheck(darker > 100, "Shadow map did not darken receiver pixels");
    {
        // 关闭背景、直接太阳光和外部 IBL，分别检查漫反射及金属镜面物体的实际像素。
        World lit;
        lit.settings.skyEnabled=false;lit.settings.diffuseIBL.clear();lit.settings.specularIBL.clear();lit.settings.brdfLut.clear();lit.settings.skySunIntensity=0;
        std::vector<Entity> spheres;
        for(int i=0;i<3;++i){auto object=lit.CreateGameObject("Sky IBL sphere");auto model=Primitive::GetModel(BuiltinMesh::Sphere);
            object->AddComponent<TransformComponent>(Vector3(float(i-1)*2.5f,0,0),Quaternion::IDENTITY,Vector3::UNIT_SCALE);
            object->AddComponent<MeshComponent>(model);object->AddComponent<MaterialComponent>(model);auto &material=object->GetComponent<MaterialComponent>().mMaterials[0];
            material.overrideModelTextures=true;material.mBaseColor=Color::White;material.mEmissive=Vector3::ZERO;material.mMetallic=i==0?0.f:1.f;material.mRoughness=i==1?.08f:.95f;
            spheres.push_back(object->GetHandle());}
        Camera eye;eye.SetLookAt(Vector3(0,1,-10),Vector3::ZERO,Vector3::UP);eye.Update();EnsureSceneViewObjects(lit,eye);
        lit.GetComponent<DirectionalLightComponent>(*lit.View<DirectionalLightComponent>().begin()).enabled=false;
        ForwardRenderingPass lightingPass;lightingPass.SetWorld(lit);lightingPass.SetCamera(&eye);
        lightingPass.SetViewportAndScissor({0,0,float(gSceneColorBuffer.GetWidth()),float(gSceneColorBuffer.GetHeight()),0,1},{0,0,LONG(gSceneColorBuffer.GetWidth()),LONG(gSceneColorBuffer.GetHeight())});
        auto draw=[&](const char *name){auto &context=GraphicsContext::Begin(L"Procedural sky PBR regression");context.TransitionResource(gSceneColorBuffer,D3D12_RESOURCE_STATE_RENDER_TARGET,true);context.ClearColor(gSceneColorBuffer);
            for(auto e:spheres)lit.GetComponent<MeshComponent>(e).Update(context,lit.GetComponent<TransformComponent>(e),lit.GetComponent<MaterialComponent>(e),0);
            lightingPass.Render(context);context.Finish(true);return readPixels(gSceneColorBuffer,name);};
        auto sample=[&](const auto &pixels,int sphere){auto position=lit.GetComponent<TransformComponent>(spheres[sphere]).transition;auto clip=Vector4(position.x,position.y,position.z,1)*eye.GetViewProjMatrix();
            int x=int((clip.x/clip.w*.5f+.5f)*gSceneColorBuffer.GetWidth()),y=int((.5f-clip.y/clip.w*.5f)*gSceneColorBuffer.GetHeight());
            Vector3 sum=Vector3::ZERO;for(int dy=-8;dy<=8;++dy)for(int dx=-8;dx<=8;++dx){auto pixel=pixels.at(size_t(y+dy)*gSceneColorBuffer.GetWidth()+x+dx);sum+=Vector3(float(pixel&1023),float((pixel>>10)&1023),float((pixel>>20)&1023));}return sum/(17.f*17);};
        lit.settings.skyZenith=lit.settings.skyHorizon=lit.settings.skyGround=Vector3(.3f,.01f,.01f);auto redSky=draw("sky-ibl-red");
        for(int i=0;i<3;++i){auto value=sample(redSky,i);FeatureCheck(value.x>30 && value.x>4*value.z,"Procedural sky does not light diffuse/specular sphere red");}
        {
            World other;SceneSerializer::FromXml(other,SceneSerializer::ToXml(lit,"Other environment"));
            other.settings.skyZenith=other.settings.skyHorizon=other.settings.skyGround=Vector3(.01f,.01f,.3f);
            ForwardRenderingPass otherPass;otherPass.SetWorld(other);otherPass.SetCamera(&eye);otherPass.SetViewportAndScissor({0,0,float(gSceneColorBuffer.GetWidth()),float(gSceneColorBuffer.GetHeight()),0,1},{0,0,LONG(gSceneColorBuffer.GetWidth()),LONG(gSceneColorBuffer.GetHeight())});
            auto &context=GraphicsContext::Begin(L"Other sky environment");for(auto e:other.View<MeshComponent,TransformComponent,MaterialComponent>())other.GetComponent<MeshComponent>(e).Update(context,other.GetComponent<TransformComponent>(e),other.GetComponent<MaterialComponent>(e),0);otherPass.Render(context);context.Finish(true);
            auto isolated=draw("sky-ibl-interleaved");for(int i=0;i<3;++i){auto value=sample(isolated,i);FeatureCheck(value.x>4*value.z,"Interleaved render views overwrote cached sky lighting descriptors");}
        }
        lit.settings.skyZenith=lit.settings.skyHorizon=lit.settings.skyGround=Vector3(.01f,.01f,.3f);auto blueSky=draw("sky-ibl-blue");
        for(int i=0;i<3;++i){auto value=sample(blueSky,i);FeatureCheck(value.z>30 && value.z>4*value.x,"Procedural sky color did not update PBR lighting");}
        lit.settings.skyIntensity=.5f;auto dimSky=draw("sky-ibl-dim");for(int i=0;i<3;++i){auto full=sample(blueSky,i),dim=sample(dimSky,i);FeatureCheck(dim.z<full.z*.6f && dim.z>full.z*.4f,"Sky brightness not reflected linearly in IBL");}
        lit.settings.iblIntensity=0;auto noIBL=draw("sky-ibl-disabled");for(int i=0;i<3;++i)FeatureCheck(sample(noIBL,i).Length()<3,"IBL intensity zero did not disable sky lighting");
        lit.settings.iblIntensity=1;lit.settings.skyIntensity=1;lit.settings.skyZenith={.7f,.1f,.01f};lit.settings.skyHorizon={.01f,.6f,.01f};lit.settings.skyGround={.01f,.01f,.7f};
        auto smooth=draw("sky-ibl-smooth");lit.GetComponent<MaterialComponent>(spheres[1]).mMaterials[0].mRoughness=.95f;auto rough=draw("sky-ibl-rough");
        FeatureCheck((sample(smooth,1)-sample(rough,1)).Length()>10,"Roughness did not select GGX-filtered sky mips");
        WriteLog(LogLevel::Info,"Smoke","Procedural sky lights diffuse/smooth/rough metal pixels; color, brightness, IBL strength and roughness passed");
    }
    WriteLog(LogLevel::Info, "Smoke",
             "Material presets, slot application, undo/redo, PBR preview pixels and shadow receiver pixels passed; "
             "darker pixels=" +
                 std::to_string(darker));
}
