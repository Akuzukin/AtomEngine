#include <fstream>
#pragma once
#include "Editor/EditorApp.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Function/Input/Input.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "imgui_internal.h"
#include "GameplayToolsSmokeChecks.h"
#include "EngineCompletionSmokeChecks.h"
namespace AtomEngine::DX12Core { extern eResolution NativeResolution; }
class PlayWorkflowSmokeApp final : public AtomEngine::EditorApp
{
    int frame = 0;
    RECT original{};
    AtomEngine::Entity selected = entt::null;
  public:
    void Initialize() override
    {
        EditorApp::Initialize();
        ImGui::GetIO().IniFilename = nullptr;
        RunGameplayToolsSmokeChecks();
        RunEngineCompletionSmokeChecks();
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        using namespace AtomEngine;
        World fixture;
        auto target = fixture.CreateGameObject("Player"); target->AddComponent<TransformComponent>();
        target->GetComponent<PersistentIdComponent>().value = "Player";
        target->AddComponent<MeshSourceComponent>();
        target->GetComponent<MeshSourceComponent>().primitive = BuiltinMesh::Cube;
        auto hud = fixture.CreateGameObject("HUD"); hud->AddComponent<TransformComponent>(); hud->AddComponent<UiElementComponent>();
        hud->AddComponent<ScriptComponent>(); hud->GetComponent<ScriptComponent>().scripts.push_back({"Scripts/AtomScripts.dll", "Game.RuntimeApiProbe", true, {}});
        auto marker=fixture.CreateGameObject("Overlay anchor marker");marker->AddComponent<TransformComponent>();marker->AddComponent<UiElementComponent>();
        auto &markerUI=marker->GetComponent<UiElementComponent>();markerUI.responsive=true;markerUI.anchorMin=markerUI.anchorMax={1,1};markerUI.pivot={1,1};markerUI.position={-16,-16};markerUI.size={40,40};markerUI.background=Color(0,1,0,1);markerUI.text="";markerUI.layer=1000;
        auto path = EngineDirectory() / "Smoke/PlayWorkflow/Preview.scene";
        SceneSerializer::Save(fixture, path, "Preview regression");
        SceneSerializer::Load(GetWorld(), path);
        mEditor.Initialize(path);
        mEditor.SetMaximizeOnPlay(true);
        selected = GetWorld().FindByName("Player");
        mEditor.Select(GetWorld(), selected);
        mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        GetWindowRect(AtomEngine::WindowManager::GetInstance()->GetWindowHandle(), &original);
        auto prepare=AtomEngine::gContext.prepareEditorViewport;
        AtomEngine::gContext.prepareEditorViewport=[this,prepare](GraphicsContext &ctx,ColorBuffer &display){
            FeatureCheck(gOverlayBuffer.GetWidth()==display.GetWidth()&&gOverlayBuffer.GetHeight()==display.GetHeight(),"Primary Overlay dimensions differ from final composite target");
            if(frame==10||frame==16||frame==27||frame==42){
                auto entity=GetWorld().FindByName("Overlay anchor marker");auto rect=UiSystem::Resolve(GetWorld(),entity);
                ctx.Flush(true);
                auto path=EngineDirectory()/"Smoke"/("overlay-composite-"+std::to_string(frame)+".raw");display.ExportToFile(path.wstring());
                ctx.TransitionResource(display,D3D12_RESOURCE_STATE_RENDER_TARGET,true);
                std::ifstream file(path,std::ios::binary);uint32_t header[4]{};file.read(reinterpret_cast<char*>(header),sizeof(header));
                uint32_t x=uint32_t(rect.position.x+rect.size.x*.5f),y=uint32_t(rect.position.y+rect.size.y*.5f),pixel=0;
                FeatureCheck(header[0]==24&&x<header[2]&&y<header[3],"Composite capture coordinates invalid");
                file.seekg(16+uint64_t(y)*header[1]+uint64_t(x)*4);file.read(reinterpret_cast<char*>(&pixel),4);
                FeatureCheck((pixel&1023)<20&&((pixel>>10)&1023)>1000&&((pixel>>20)&1023)<20,"GPU composite did not place UI at its resolved bottom-right anchor");
            }
            prepare(ctx,display);
        };

    }
    float RuntimeDeltaTime(float) const override { return 1.f / 30; }
    void BeginFrame() override
    {
        if (frame == 45 || frame == 46) ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, frame == 45);
        EditorApp::BeginFrame();
    }
    void OnRuntimeError(const std::string &error) override { FeatureCheck(error.empty(), error.c_str()); }
    void Update(float dt) override
    {
        using namespace AtomEngine;
        auto window = WindowManager::GetInstance();
        auto hwnd = window->GetWindowHandle();
        if (frame == 3 || frame == 8 || frame == 13)
            SetWindowPos(hwnd, nullptr, 0, 0, 900 + frame * 10, 600 + frame * 5, SWP_NOMOVE | SWP_NOZORDER);
        if (frame == 4 || frame == 9 || frame == 14)
        {
            RECT client{}; GetClientRect(hwnd, &client);
            FeatureCheck(DX12Core::gBackBufferWidth == client.right && DX12Core::gBackBufferHeight == client.bottom,
                         "Frame-boundary swapchain resize failed");
        }
        if(frame==6)DX12Core::NativeResolution=DX12Core::k720p;
        if(frame==11)DX12Core::NativeResolution=DX12Core::k1080p;
        if (frame == 18) window->SetFullscreen(true);
        if (frame == 20) FeatureCheck(window->IsFullscreen(), "Fullscreen entry failed");
        if (frame == 23) window->SetFullscreen(false);
        if (frame == 26) FeatureCheck(!window->IsFullscreen(), "Fullscreen exit failed");
        if (frame == 28) ShowWindow(hwnd, SW_MINIMIZE);
        if (frame == 30) ShowWindow(hwnd, SW_RESTORE);
        if (frame == 35)
        {
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
            mEditor.SetMaximizeOnPlay(false);
        }
        if(frame==36)mEditor.Select(GetWorld(),GetWorld().FindByName("HUD"));
        if(frame==44)mEditor.Select(GetWorld(),selected);
        EditorApp::Update(dt);
        if(frame>=3 && frame<85) {
            auto render=GetWorld().uiRenderSize;
            FeatureCheck(render.x==float(gOverlayBuffer.GetWidth()) && render.y==float(gOverlayBuffer.GetHeight()),"Editor UI layout uses scene buffer instead of UI composite buffer");
        }

        if (mEditor.IsPlaying())
            FeatureCheck(!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard), "Play stole keyboard navigation");
        if (frame >= 36 && frame < 55)
        {
            auto main = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(main->Pos.x + main->Size.x + 30, main->Pos.y + 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(300, 200));
            ImGui::Begin("Detached input regression", nullptr, ImGuiWindowFlags_NoDocking);
            auto detached = (HWND)ImGui::GetWindowViewport()->PlatformHandle;
            if (frame == 42 && detached) SetForegroundWindow(detached);
            if (frame == 43)
                FeatureCheck(detached && window->additionalInputWindow(detached), "Detached input window not recognized");
            ImGui::TextUnformatted("Secondary viewport focus regression");
            ImGui::End();
        }
        if (frame == 46) FeatureCheck(mEditor.Selected() == selected, "Game key triggered editor selection shortcut");
        if (frame == 56)
        {
            SetForegroundWindow(hwnd);
            mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        }
        if (frame == 58) {
            FeatureCheck(Audio::GetInstance()->ActiveVoiceCount() == 0, "Stop left audio playing");
            FeatureCheck(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard, "Editor navigation not restored");
            SetWindowPos(hwnd, nullptr, original.left, original.top, original.right-original.left,
                         original.bottom-original.top, SWP_NOZORDER);
            WriteLog(LogLevel::Info, "Smoke", "Play resize/fullscreen/minimize, managed query/visibility, navigation and detached input ownership passed");
        }
        if (frame == 59)
        {
            // Rebuild the ECS script pool independently of fixture ordering.
            // Restore/reload must not affect repeated startup.
            std::vector<std::pair<Entity, ScriptComponent>> attachments;
            for (auto e : GetWorld().View<ScriptComponent>())
                attachments.emplace_back(e, GetWorld().GetComponent<ScriptComponent>(e));
            for (auto &[e, component] : attachments) GetWorld().RemoveComponent<ScriptComponent>(e);
            auto hud = GetWorld().FindByName("HUD"), player = GetWorld().FindByName("Player");
            for (auto &[e, component] : attachments)
                if (e != hud && e != player) GetWorld().AddComponent<ScriptComponent>(e, component);
            for (auto target : {hud, player})
                for (auto &[e, component] : attachments)
                    if (e == target) GetWorld().AddComponent<ScriptComponent>(e, component);
        }
        if (frame == 60 || frame == 74) mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        if (frame == 64 || frame == 78)
        {
            auto hud = GetWorld().FindByName("HUD");
            FeatureCheck(GetWorld().GetComponent<UiElementComponent>(hud).text.starts_with("Frames:"), "Scripts did not update after multiviewport and repeated Play");
        }
        if (frame == 70 || frame == 84) mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        if (frame == 86) WriteLog(LogLevel::Info, "Smoke", "Repeated Play after multiviewport with reordered script initialization updated scripts");
        ++frame;
    }
    void Shutdown() override
    {
        AtomEngine::WindowManager::GetInstance()->SetFullscreen(false);
        EditorApp::Shutdown();
    }
};
