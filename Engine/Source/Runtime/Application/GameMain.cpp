#include "GameApp.h"
#include "Runtime/EngineCore.h"
#include "Runtime/Function/Scene/GameProject.h"
#include "Runtime/Function/Scene/SceneView.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Resource/AssetArchive.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Resource/MaterialPreset.h"
#include "Runtime/Function/Audio/Audio.h"
#include <fstream>
#include <shellapi.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include "json.hpp"
#pragma comment(lib, "shell32.lib")

namespace AtomEngine
{
class PublishedGame final : public GameApp
{
    bool smoke = false;
    int frame = 0;
public:
    explicit PublishedGame(bool testing = false) : smoke(testing) {}
    void Initialize() override
    {
        ActiveProject.game = GameLaunchSettings::Load();
        auto& project = *ActiveProject.game;
        ProjectSceneDefaults = project.defaults;
        ActiveProject.currentScene = project.startupScene;
        gContext.showGrid = gContext.showColliders = gContext.showRenderDiagnostics = false;
        auto window = gContext.windowManager;
        SetWindowTextW(window->GetWindowHandle(), UTF8ToWString(project.name).c_str());
        RECT rect{0, 0, project.width, project.height};
        AdjustWindowRect(&rect, GetWindowLongW(window->GetWindowHandle(), GWL_STYLE), FALSE);
        SetWindowPos(window->GetWindowHandle(), nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
        window->SetFullscreen(project.fullscreen);
        mSceneManager.LoadScene(UTF8ToWString(project.startupScene)); mSceneManager.Update(0);
        ApplySceneCamera(GetWorld(), GetCamera());
    }
    void Update(float dt) override
    {
        mSceneManager.Update(dt);
        if (!smoke) return;
        auto& project = *ActiveProject.game;
        if (frame == 10 && AssetFileExists("Asset/Prefabs/Spawn.prefab")) {
            SceneSerializer::InstantiatePrefab(GetWorld(), "Asset/Prefabs/Spawn.prefab");
            gContext.audio->PlayClip("Asset/Sounds/se.mp3", 0.1f, false);
            if (AssetFileExists("Asset/Materials/RedPlastic.mat")) MaterialPreset::Load("Asset/Materials/RedPlastic.mat");
            WriteLog(LogLevel::Info,"Game","Packaged prefab, material and audio memory loading passed");
        }
        if (++frame == 20 && project.scenes.size() > 1) ActiveProject.RequestScene(project.scenes.back());
        if (frame == 75 && project.scenes.size() > 1)
        {
            if (ActiveProject.currentScene != project.scenes.back()) throw std::runtime_error("Published game scene switch failed");
            ActiveProject.RequestScene(project.startupScene);
        }
        if (frame == 100)
        {
            if (ActiveProject.currentScene != project.startupScene || ActiveProject.project || gContext.renderOverlay || gContext.prepareEditorViewport)
                throw std::runtime_error("Published game restoration/editor isolation failed");
            WriteLog(LogLevel::Info, "Game", "Published game scene switching and editor isolation passed");
        }
    }
    void Render() override {}
    bool Exit() override { return false; }
    void Shutdown() override { mSceneManager.Shutdown(); ActiveProject = {}; }
    void OnRuntimeError(const std::string& error) override
    {
        if (!error.empty()) throw std::runtime_error(error);
    }
    void OnSceneLoadError(const std::string& error) override { if (smoke) throw std::runtime_error(error); }
};
}
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Engine::AtomEngine engine;
    auto commandLine = std::wstring_view(GetCommandLineW());
    bool headless = commandLine.find(L"--smoke-test") != std::wstring_view::npos || commandLine.find(L"--model-dependencies") != std::wstring_view::npos || commandLine.find(L"--inspect-package") != std::wstring_view::npos;
    try
    {
        // 从任意工作目录启动均以发行目录解析项目和资源。
        std::filesystem::current_path(AtomEngine::EngineDirectory());
        uint32_t frames = 0;
        int count = 0; auto args = CommandLineToArgvW(GetCommandLineW(), &count);
        if (count == 3 && std::wstring_view(args[1]) == L"--model-dependencies") {
            Assimp::Importer importer; auto model = importer.ReadFile(AtomEngine::WStringToUTF8(args[2]), 0);
            if (!model) { LocalFree(args); throw std::runtime_error(importer.GetErrorString()); }
            nlohmann::json paths = nlohmann::json::array();
            for (unsigned m = 0; m < model->mNumMaterials; ++m)
                for (int type = 1; type <= int(aiTextureType_UNKNOWN); ++type)
                    for (unsigned t = 0; t < model->mMaterials[m]->GetTextureCount(aiTextureType(type)); ++t) {
                        aiString path; if (model->mMaterials[m]->GetTexture(aiTextureType(type), t, &path) == AI_SUCCESS) paths.push_back(path.C_Str());
                    }
            auto json = paths.dump(); DWORD written = 0; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), json.data(), DWORD(json.size()), &written, nullptr);
            LocalFree(args); return 0;
        }
        bool inspect = false;
        for (int i = 1; i < count; ++i) { if (std::wstring_view(args[i]) == L"--smoke-test") frames = 120; if (std::wstring_view(args[i]) == L"--inspect-package") inspect = true; }
        LocalFree(args);
        wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable)));
        AtomEngine::MountGameArchive(executable, AtomEngine::EngineDirectory() / "game.atpak");
        if (inspect) {
            auto names = AtomEngine::GameArchiveEntries();
            for (auto& name : names) AtomEngine::ReadAssetBytes(AtomEngine::UTF8ToWString(name));
            AtomEngine::GameLaunchSettings::Load();
            auto json = nlohmann::json(names).dump(); DWORD written=0; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),json.data(),DWORD(json.size()),&written,nullptr);
            AtomEngine::UnmountGameArchive(); return 0;
        }
        AtomEngine::PublishedGame app(frames != 0); engine.Run(app, frames); engine.Shutdown(); AtomEngine::UnmountGameArchive();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::ofstream(AtomEngine::EngineDirectory() / "runtime-error.txt") << error.what();
        AtomEngine::WriteLog(AtomEngine::LogLevel::Critical, "Game", error.what()); AtomEngine::FlushLog();
        DWORD written = 0; WriteFile(GetStdHandle(STD_ERROR_HANDLE), error.what(), DWORD(strlen(error.what())), &written, nullptr);
        if (!headless)
            MessageBoxW(nullptr, AtomEngine::UTF8ToWString(error.what()).c_str(), L"Atom game", MB_ICONERROR);
        return 1;
    }
}
