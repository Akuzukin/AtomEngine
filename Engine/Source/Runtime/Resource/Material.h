#pragma once
#include "Runtime/Core/Math/MathInclude.h"
#include <array>

namespace AtomEngine
{
enum TextureSlot : uint8_t
{
    kBaseColor = 1 << 0,
    kMetallic = 1 << 1,
    kRoughness = 1 << 2,
    kNormal = 1 << 3,
    kOcclusion = 1 << 4,
    kEmissive = 1 << 5,
    kMetallicRoughness = 1 << 6,
    kSpecular = 1 << 7,
};

struct MaterialTexture
{
    std::wstring name;
    TextureSlot slot;
};

enum class ShadingModel
{
    MetallicRoughness,
    Glass,
    Cloth
};
struct MaterialInstance
{
    Color mBaseColor = Color::White;
    Vector3 mEmissive{0, 0, 0};
    float mMetallic = 0, mRoughness = 1, mNormalScale = 1, mAO = 1;
    Vector3 mFresnelF0{0.04f, 0.04f, 0.04f};
    Transform uvTransform;
    uint32_t useLight = 1;
    ShadingModel shadingModel = ShadingModel::MetallicRoughness;
    // GPU order: basecolor, metallic, roughness, normal, AO, emissive, specular.
    std::array<std::string, 7> texturePaths{};
    bool overrideModelTextures = false;
    // Bit 0: metallic uses B; bit 1: roughness uses G (glTF packed maps).
    uint32_t packedTextureChannels = 0;
};

struct Material
{
    std::string name;

    Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 emissiveFactor = {0.0f, 0.0f, 0.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float ao = 1.0f;

    uint8_t textureMask = 0;
    bool hasAlphaBlend = false;

    std::vector<MaterialTexture> textures;
};
} // namespace AtomEngine
