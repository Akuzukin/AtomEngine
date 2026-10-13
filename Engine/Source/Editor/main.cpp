#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/EngineCore.h"
#include "EditorApp.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include <string_view>
#include <fstream>
#include <filesystem>
#include "Tests/RuntimeSmokeChecks.h"
#include "Tests/FeatureSmokeChecks.h"
#include "Tests/EditorSmokeChecks.h"
#include "Tests/RenderCameraSmokeChecks.h"
#include "Tests/ShadowMaterialSmokeChecks.h"
#include "Tests/SceneViewSmokeChecks.h"
#include "imgui_internal.h"

static void CaptureFeatureFrame()
{
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto folder = std::filesystem::path(exe).parent_path() / "Smoke";
    AtomEngine::gSceneColorBuffer.ExportToFile((folder / L"scene.raw").wstring());
    AtomEngine::gOverlayBuffer.ExportToFile((folder / L"ui.raw").wstring());
    std::ifstream ui(folder / L"ui.raw", std::ios::binary);
    uint32_t header[4]{};
    ui.read(reinterpret_cast<char *>(header), sizeof(header));
    FeatureCheck(header[0] == DXGI_FORMAT_R8G8B8A8_UNORM, "Unexpected UI capture format");
    std::vector<uint8_t> data(size_t(header[1]) * header[3]);
    ui.read(reinterpret_cast<char *>(data.data()), data.size());
    bool visible = false;
    for (size_t i = 3; i < data.size(); i += 4)
        visible |= data[i] > 0;
    FeatureCheck(visible, "UI text did not produce visible pixels");
}

class SmokeEditorApp final : public AtomEngine::EditorApp
{
  public:
    bool IsPlaying() const override
    {
        return true;
    }
    void OnRuntimeError(const std::string &error) override
    {
        EditorApp::OnRuntimeError(error);
        FeatureCheck(error.empty(), error.c_str());
    }
    void Initialize() override
    {
        EditorApp::Initialize();
        ImGui::GetIO().IniFilename = nullptr;
        RunRuntimeSmokeChecks();
        RunRenderCameraSmokeChecks();
        RunFeatureSmokeChecks(GetWorld(), GetCamera());
        RunShadowMaterialSmokeChecks();
        RunSceneViewSmokeChecks();
        RunEditorSmokeChecks();
        SelectObject(GetWorld().FindByName("Sphere_0_0"));
        AtomEngine::gContext.capturePresented = [this](AtomEngine::ColorBuffer &buffer) {
            if (frames == 5 || frames == 20 || frames == 50 || frames == 58 || frames == 85)
            {
                wchar_t exe[MAX_PATH];
                GetModuleFileNameW(nullptr, exe, MAX_PATH);
                auto file = frames == 5    ? "editor-folders.raw"
                            : frames == 50 ? "editor-play.raw"
                            : frames == 58 ? "editor-pause.raw"
                            : frames == 85 ? "editor-restored.raw"
                                           : "editor.raw";
                buffer.ExportToFile((std::filesystem::path(exe).parent_path() / "Smoke" / file).wstring());
                auto &context = AtomEngine::GraphicsContext::Begin(L"Restore captured back buffer");
                context.TransitionResource(buffer, D3D12_RESOURCE_STATE_PRESENT);
                context.Finish();
            }
        };
    }
    void Update(float dt) override
    {
        if (frames == 10)
            mEditor.BrowseAssets("Asset/Textures");
        if (frames == 35)
        {
            mEditor.SetMaximizeOnPlay(true);
            mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        }
        if (frames == 54)
            mEditor.TogglePause();
        if (frames == 61)
            mEditor.TogglePause();
        if (frames == 72)
            mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        EditorApp::Update(dt);
#ifndef RELEASE
        if (frames == 45)
        {
            auto window = ImGui::FindWindowByName("Play view");
            FeatureCheck(window && window->Active && window->Size.x > ImGui::GetMainViewport()->Size.x - 2,
                         "Play did not fill the app window");
        }
#endif
        if (frames == 57)
            FeatureCheck(mEditor.IsPaused(), "Debug pause UI state failed");
        if (frames == 73)
            FeatureCheck(!mEditor.IsPlayMaximized() && !mEditor.IsPaused(), "Stop left fullscreen/pause state active");
    }
    void Render() override
    {
        if (++frames == 20)
        {
            CaptureFeatureFrame();
        }
        EditorApp::Render();
    }

  private:
    int frames = 0;
};

class SmokeRuntimeApp final : public GameApp
{
  public:
    void Initialize() override
    {
        mSceneManager.LoadScene("Asset/Scenes/MaterialTest.scene");
        mSceneManager.Update(0);
        GetCamera().SetLookAt(Vector3(17, 20, -26), Vector3(0, 1, 0), Vector3(0, 1, 0));
        GetCamera().Update();
        RunRuntimeSmokeChecks();
        RunRenderCameraSmokeChecks();
        RunFeatureSmokeChecks(GetWorld(), GetCamera());
        RunShadowMaterialSmokeChecks();
        RunSceneViewSmokeChecks();
    }
    void Update(float dt) override
    {
        mSceneManager.Update(dt);
    }
    void Render() override
    {
        if (++frames == 20)
            CaptureFeatureFrame();
    }
    void Shutdown() override
    {
        mSceneManager.Shutdown();
    }
    void OnRuntimeError(const std::string &error) override
    {
        FeatureCheck(error.empty(), error.c_str());
    }

  private:
    int frames = 0;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{

    Engine::AtomEngine engine;

    const bool runtimeSmoke = std::string_view(lpCmdLine).find("--runtime-smoke-test") != std::string_view::npos;
    const bool smoke = runtimeSmoke || std::string_view(lpCmdLine).find("--smoke-test") != std::string_view::npos;
    std::unique_ptr<GameApp> app;
    if (runtimeSmoke)
        app = std::make_unique<SmokeRuntimeApp>();
    else if (smoke)
        app = std::make_unique<SmokeEditorApp>();
    else
        app = std::make_unique<AtomEngine::EditorApp>();

    try
    {
        engine.Run(*app, smoke ? 120 : 0);
    }
    catch (const std::exception &e)
    {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::ofstream(std::filesystem::path(exe).parent_path() / "runtime-error.txt") << e.what();
        WriteLog(LogLevel::Critical, "Startup", e.what());
        FlushLog();
        OutputDebugStringA(e.what());
        if (!smoke)
            MessageBoxA(nullptr, e.what(), "AtomEngine startup failed", MB_OK | MB_ICONERROR);
        return 1;
    }

    engine.Shutdown();
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::error_code ignored;
    std::filesystem::remove(std::filesystem::path(exe).parent_path() / "runtime-error.txt", ignored);
    return 0;
}
