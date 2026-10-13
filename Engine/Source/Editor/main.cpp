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
#include "Tests/ColorSpaceSmokeChecks.h"
#include "imgui_internal.h"
#include "Tests/LocalizationSmokeChecks.h"

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

#include "Tests/EnhancementSmokeChecks.h"
class SmokeEditorApp final : public AtomEngine::EditorApp
{
  public:
    explicit SmokeEditorApp(bool colorOnly = false, bool enhancements = false)
        : mColorOnly(colorOnly), mEnhancements(enhancements)
    {
    }
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
        if (!mColorOnly)
            RunFeatureSmokeChecks(GetWorld(), GetCamera());
        RunShadowMaterialSmokeChecks();
        RunSceneViewSmokeChecks();
        RunColorSpaceSmokeChecks();
        RunEditorSmokeChecks();
        RunLocalizationSmokeChecks();
        if (mEnhancements)
            RunEnhancementSmokeChecks();
        SelectObject(GetWorld().FindByName("Sphere_0_0"));
        uint32_t sample = 0xff808080;
        mColorSample.Create2D(4, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, &sample);
        mColorSampleDescriptor = AtomEngine::Renderer::GetTextureHeap().Alloc();
        AtomEngine::DX12Core::gDevice->CopyDescriptorsSimple(1, mColorSampleDescriptor, mColorSample.GetSRV(),
                                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        AtomEngine::gContext.capturePresented = [this](AtomEngine::ColorBuffer &buffer) {
            if (frames == 5 || frames == 20 || frames == 30 || frames == 50 || frames == 58 || frames == 85)
            {
                wchar_t exe[MAX_PATH];
                GetModuleFileNameW(nullptr, exe, MAX_PATH);
                auto file = frames == 5    ? "editor-folders.raw"
                            : frames == 30 ? "editor-english.raw"
                            : frames == 50 ? "editor-play.raw"
                            : frames == 58 ? "editor-pause.raw"
                            : frames == 85 ? "editor-restored.raw"
                                           : "editor.raw";
                buffer.ExportToFile((std::filesystem::path(exe).parent_path() / "Smoke" / file).wstring());
                if (frames == 20)
                {
                    std::ifstream image(std::filesystem::path(exe).parent_path() / "Smoke" / file, std::ios::binary);
                    uint32_t header[4]{};
                    image.read(reinterpret_cast<char *>(header), 16);
                    std::vector<uint32_t> data(size_t(header[1] / 4) * header[3]);
                    image.read(reinterpret_cast<char *>(data.data()), data.size() * 4);
                    float a = 128.f / 255, low = 64.f / 255;
                    float expected[] = {SmokeSRGB(low), SmokeSRGB(low * (1 - a) + a * a), 128.f / 255, SmokeSRGB(a),
                                        SmokeSRGB(low)};
                    for (int i = 0; i < 5; ++i)
                    {
                        int x = int((48 + i * 72) * ImGui::GetIO().DisplayFramebufferScale.x),
                            y = int(72 * ImGui::GetIO().DisplayFramebufferScale.y);
                        float pixel = float(data.at(size_t(y) * (header[1] / 4) + x) & 1023) / 1023;
                        FeatureCheck(std::abs(pixel - expected[i]) < .008f,
                                     "ImGui gamma/alpha/image encoding regression");
                    }
                    AtomEngine::WriteLog(AtomEngine::LogLevel::Info, "Smoke",
                                         "ImGui linear colors/alpha and encoded preview preservation passed");
                }

                auto &context = AtomEngine::GraphicsContext::Begin(L"Restore captured back buffer");
                context.TransitionResource(buffer, D3D12_RESOURCE_STATE_PRESENT);
                context.Finish();
            }
        };
    }
    void Update(float dt) override
    {
        if (frames == 22)
            AtomEngine::EditorLocalization::SetLanguage("en");
        if (frames == 78)
            AtomEngine::EditorLocalization::SetLanguage("zh-CN");
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
        if (frames > 90 && frames < 110)
        {
            auto main = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(main->Pos.x + main->Size.x + 24, main->Pos.y + 40), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(240, 120));
            ImGui::Begin("Color detached regression", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking);
            ImGui::TextUnformatted("SRGB secondary viewport");
            ImGui::ColorButton("Linear grey", ImVec4(.25f, .25f, .25f, 1));
            ImGui::End();
            if (frames == 100)
            {
                auto *detached = ImGui::FindWindowByName("Color detached regression");
                FeatureCheck(detached && detached->Viewport->ID != main->ID && detached->Viewport->PlatformHandle,
                             "Secondary ImGui color viewport was not created");
            }
        }

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
        if (frames == 20)
        {
            auto *draw = ImGui::GetForegroundDrawList();
            auto origin = ImGui::GetMainViewport()->Pos;
            auto low = [&](int i) { return ImVec2(origin.x + 16 + i * 72, origin.y + 48); };
            auto high = [&](int i) { return ImVec2(origin.x + 80 + i * 72, origin.y + 96); };
            draw->AddRectFilled(low(0), high(0), IM_COL32(64, 64, 64, 255));
            draw->AddRectFilled(low(1), high(1), IM_COL32(64, 64, 64, 255));
            draw->AddRectFilled(low(1), high(1), IM_COL32(128, 128, 128, 128));
            AtomEngine::ImGuiCommon::AddImageDisplayEncoded(*draw, (ImTextureID)mColorSampleDescriptor.GetGpuPtr(),
                                                            low(2), high(2));
            draw->AddImage((ImTextureID)mColorSampleDescriptor.GetGpuPtr(), low(3), high(3));
            draw->AddRectFilled(low(4), high(4), IM_COL32(64, 64, 64, 255));
        }
        EditorApp::Render();
    }

    void Shutdown() override
    {
        AtomEngine::Renderer::GetTextureHeap().Free(mColorSampleDescriptor);
        mColorSample.Destroy();
        EditorApp::Shutdown();
    }

  private:
    AtomEngine::Texture mColorSample;
    AtomEngine::DescriptorHandle mColorSampleDescriptor;
    bool mColorOnly = false;
    bool mEnhancements = false;
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
        RunColorSpaceSmokeChecks();
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
    const bool colorSmoke = std::string_view(lpCmdLine).find("--ui-color-smoke-test") != std::string_view::npos;
    const bool enhancements = std::string_view(lpCmdLine).find("--enhancement-smoke-test") != std::string_view::npos;
    const bool smoke = enhancements || colorSmoke || runtimeSmoke ||
                       std::string_view(lpCmdLine).find("--smoke-test") != std::string_view::npos;
    std::unique_ptr<GameApp> app;
    if (runtimeSmoke)
        app = std::make_unique<SmokeRuntimeApp>();
    else if (smoke)
        app = std::make_unique<SmokeEditorApp>(colorSmoke || enhancements, enhancements);
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
