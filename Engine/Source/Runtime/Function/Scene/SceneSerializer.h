#pragma once
#include <functional>
#include "Runtime/Function/Framework/ECS/World.h"
#include <filesystem>
#include "Runtime/Resource/AssetManager.h"
namespace AtomEngine
{
class SceneSerializer
{
  public:
    static std::string ToXml(World &world, const std::string &name);
    static std::string FromXml(World &world, std::string_view xml);
    static Entity Duplicate(World &world, Entity root);
    static std::vector<AssetRequest> CollectResources(const std::filesystem::path &path);
    static void Save(World &world, const std::filesystem::path &path, const std::string &name);
    static std::string Load(World &world, const std::filesystem::path &path, const std::function<void()>& beforeCommit = {});
    static void SavePrefab(World &world, Entity root, const std::filesystem::path &path);
    static void AttachPrefab(World &world,Entity target,const std::filesystem::path &path);
    static Entity InstantiatePrefab(World &world, const std::filesystem::path &path);
    static void RefreshPrefabs(World &world, const std::filesystem::path &source = {});
    static void RevertPrefab(World &world, Entity instance);
    static void ApplyPrefab(World &world, Entity instance);
    static size_t PrefabOverrideCount(World &world, Entity instance);
    static std::vector<std::string> PrefabOverrides(World &world, Entity instance);
    static void ResolveHierarchy(World &world);
    static void RebuildMesh(World &world, Entity entity);
};
} // namespace AtomEngine
