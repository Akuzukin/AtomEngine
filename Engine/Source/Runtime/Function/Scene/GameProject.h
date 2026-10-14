#pragma once
#include "ProjectSettings.h"
#include <optional>

namespace AtomEngine
{
// .atproj 的路径均相对于项目文件所在目录；场景列表同时是游戏的构建清单。
struct GameProject
{
    std::filesystem::path file;
    ProjectSettings settings;
    std::vector<std::string> scenes, additionalAssets;
    std::string scriptProject = "Managed/GameScripts/GameScripts.csproj";
    std::string buildOutput = "Build/Windows";
    int width = 1280, height = 720;
    bool fullscreen = false;
    static GameProject Load(const std::filesystem::path& path);
    void Save() const;
    void Validate(bool requireFiles = true) const;
    std::filesystem::path Root() const { return file.parent_path(); }
    std::string FindScene(const std::string& nameOrPath) const;
    static std::string RelativePath(const std::filesystem::path& path);
};

// 场景切换请求在下一帧处理，避免销毁当前正在执行的脚本或物理回调。
struct GameLaunchSettings
{
    std::string name, startupScene;
    std::vector<std::string> scenes;
    SceneSettings defaults;
    int width = 1280, height = 720;
    bool fullscreen = false;
    static GameLaunchSettings Load();
};
struct ProjectSession
{
    std::optional<GameProject> project;
    std::optional<GameLaunchSettings> game;
    std::string currentScene;
    std::string lastLoadError;
    std::optional<std::string> pendingScene;
    bool dirty = false;
    void RequestScene(const std::string& nameOrPath);
    const std::vector<std::string>& ScenePaths() const;
    std::filesystem::path SceneRoot() const;
};
inline ProjectSession ActiveProject;
std::filesystem::path EngineDirectory();
std::filesystem::path ResolveAssetFile(const std::filesystem::path& path);
}
