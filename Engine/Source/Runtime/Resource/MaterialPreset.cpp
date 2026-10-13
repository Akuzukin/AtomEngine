#include "MaterialPreset.h"
#include "AssetManager.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "Runtime/Function/Framework/Component/MaterialComponent.h"
#include "External/tinyxml2/tinyxml2.h"
#include "Runtime/Function/Framework/Component/MeshComponent.h"
#include "Runtime/Function/Render/ModelRenderResources.h"
namespace AtomEngine
{
namespace
{
MaterialInstance Decode(const tinyxml2::XMLElement *node)
{
    tinyxml2::XMLDocument doc;
    auto root = doc.NewElement("Scene");
    root->SetAttribute("version", 1);
    doc.InsertEndChild(root);
    auto object = doc.NewElement("Object");
    object->SetAttribute("id", "material-preset");
    root->InsertEndChild(object);
    object->InsertEndChild(node->DeepClone(&doc));
    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    World world;
    SceneSerializer::FromXml(world, printer.CStr());
    return world.GetComponent<MaterialComponent>(*world.View<MaterialComponent>().begin()).mMaterials.at(0);
}
} // namespace
MaterialPreset MaterialPreset::Load(const std::filesystem::path &path)
{
    if (path.extension() != L".mat")
        throw std::runtime_error("Material presets must use .mat");
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error("Cannot load material preset: " + path.string());
    auto root = doc.FirstChildElement("MaterialPreset");
    if (!root || root->IntAttribute("version") != 1 || !root->FirstChildElement("Material"))
        throw std::runtime_error("Invalid material preset");
    MaterialPreset result;
    if (auto name = root->Attribute("name"))
        result.name = name;
    result.material = Decode(root->FirstChildElement("Material"));
    result.material.overrideModelTextures = true;
    if (result.material.shadingModel != ShadingModel::MetallicRoughness)
        throw std::runtime_error("Unsupported material shading model");
    return result;
}
MaterialPreset MaterialPreset::FromObject(World &world, Entity object, int slot)
{
    auto component = world.TryGet<MaterialComponent>(object);
    if (!component || slot < 0 || slot >= int(component->mMaterials.size()))
        throw std::runtime_error("Invalid material slot");
    MaterialPreset preset;
    preset.material = component->mMaterials.at(slot);
    auto mesh = world.TryGet<MeshComponent>(object);
    auto model = mesh ? mesh->GetModel() : nullptr;
    if (model && !preset.material.overrideModelTextures && slot < int(model->mMaterials.size()))
    {
        const auto &source = model->mMaterials.at(slot);
        const TextureSlot slots[] = {kBaseColor, kMetallic, kRoughness, kNormal, kOcclusion, kEmissive, kSpecular};
        auto refs = model->renderResources->textures.find(slot);
        if (refs != model->renderResources->textures.end())
            for (size_t i = 0; i < 7; ++i)
            {
                if (!preset.material.texturePaths[i].empty())
                    continue;
                auto target =
                    ((i == 1 || i == 2) && (source.textureMask & kMetallicRoughness)) ? kMetallicRoughness : slots[i];
                for (size_t t = 0; t < source.textures.size() && t < refs->second.size(); ++t)
                {
                    if (source.textures[t].slot != target || !refs->second[t].IsValid())
                        continue;
                    auto path = std::filesystem::path(refs->second[t].SourcePath());
                    auto relative = path.lexically_relative(std::filesystem::current_path());
                    preset.material.texturePaths[i] =
                        WStringToUTF8((relative.empty() ? path : relative).generic_wstring());
                    if (target == kMetallicRoughness)
                        preset.material.packedTextureChannels |= 1u << (i - 1);
                    break;
                }
            }
    }
    preset.material.overrideModelTextures = true;
    return preset;
}
void MaterialPreset::Save(const std::filesystem::path &path) const
{
    if (material.shadingModel != ShadingModel::MetallicRoughness)
        throw std::runtime_error("Unsupported material shading model");
    if (path.extension() != L".mat")
        throw std::runtime_error("Material presets must use .mat");
    World world;
    auto object = world.CreateGameObject("Material");
    object->AddComponent<MaterialComponent>();
    object->GetComponent<MaterialComponent>().mMaterials[0] = material;
    tinyxml2::XMLDocument scene;
    scene.Parse(SceneSerializer::ToXml(world, "Material").c_str());
    tinyxml2::XMLDocument doc;
    auto root = doc.NewElement("MaterialPreset");
    root->SetAttribute("version", 1);
    root->SetAttribute("name", name.c_str());
    doc.InsertEndChild(root);
    root->InsertEndChild(
        scene.RootElement()->FirstChildElement("Object")->FirstChildElement("Material")->DeepClone(&doc));
    Decode(root->FirstChildElement("Material"));
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L".tmp";
    if (doc.SaveFile(temporary.string().c_str()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error("Cannot save material preset");
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace material preset");
}
void MaterialPreset::Preload() const
{
    std::vector<AssetRequest> requests;
    for (size_t i = 0; i < 7; ++i)
        if (!material.texturePaths[i].empty())
            requests.push_back({AssetKind::Texture, material.texturePaths[i], BuiltinMesh::None,
                                i == 3 ? kDefaultNormalMap : kWhiteOpaque2D, i == 0 || i == 5});
    auto batch = AssetManager::Preload(std::move(requests));
    if (!batch->Report().Success())
        throw std::runtime_error(batch->Report().errors.front());
}
void MaterialPreset::Apply(World &w, Entity e, int slot) const
{
    auto component = w.TryGet<MaterialComponent>(e);
    if (!component || component->mMaterials.empty())
        throw std::runtime_error("Drop material onto a mesh object");
    if (slot >= int(component->mMaterials.size()) || slot < -1)
        throw std::runtime_error("Invalid material slot");
    Preload();
    auto copy = material;
    copy.overrideModelTextures = true;
    if (slot >= 0)
        component->mMaterials.at(slot) = copy;
    else
        for (auto &instance : component->mMaterials)
            instance = copy;
}
} // namespace AtomEngine
