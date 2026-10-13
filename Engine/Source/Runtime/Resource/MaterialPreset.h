#pragma once
#include "Material.h"
#include "Runtime/Function/Framework/ECS/World.h"
#include <filesystem>
namespace AtomEngine
{
struct MaterialPreset
{
    std::string name = "Material";
    MaterialInstance material;
    static MaterialPreset FromObject(World &world, Entity object, int slot);
    static MaterialPreset Load(const std::filesystem::path &path);
    void Save(const std::filesystem::path &path) const;
    void Apply(World &world, Entity object, int slot = -1) const;
    void Preload() const;
};
} // namespace AtomEngine
