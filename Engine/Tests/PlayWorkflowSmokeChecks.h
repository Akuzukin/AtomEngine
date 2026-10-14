#pragma once
#include "Editor/EditorApp.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Function/Input/Input.h"
#include "Runtime/Function/Scene/SceneSerializer.h"
#include "Runtime/Function/Framework/ECS/GameObject.h"
#include "imgui_internal.h"
#include "GameplayToolsSmokeChecks.h"
#include "EngineCompletionSmokeChecks.h"
class PlayWorkflowSmokeApp final : public AtomEngine::EditorApp
{
    int frame = 0;
    RECT original{};
    bool deathAudioChecked = false;
    int lastKills = 0;
    AtomEngine::Entity selected = entt::null;
  public:
    void Initialize() override
    {
        EditorApp::Initialize();
        ImGui::GetIO().IniFilename = nullptr;
        RunGameplayToolsSmokeChecks();
        RunEngineCompletionSmokeChecks();
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        AtomEngine::SceneSerializer::Load(GetWorld(), "Asset/Scenes/Survivors.scene");
        auto hud = GetWorld().FindByName("HUD");
        GetWorld().GetComponent<AtomEngine::ScriptComponent>(hud).scripts.push_back(
            {"Scripts/AtomScripts.dll", "Game.SurvivorApiProbe", true, {}});
        mEditor.Initialize("Asset/Scenes/Survivors.scene");
        mEditor.SetMaximizeOnPlay(true);
        selected = GetWorld().FindByName("Player");
        mEditor.Select(GetWorld(), selected);
        mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        GetWindowRect(AtomEngine::WindowManager::GetInstance()->GetWindowHandle(), &original);
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
        if (frame == 14) {
            auto player = GetWorld().FindByName("Player");
            for (int n=0;n<64;++n) {
                auto e=GetWorld().FindByName("Enemy"+std::to_string(n));
                if (GetWorld().GetComponent<MeshSourceComponent>(e).visible) {
                    GetWorld().GetComponent<TransformComponent>(e).transition =
                        GetWorld().GetComponent<TransformComponent>(player).transition + Vector3(2,0,0);
                    break;
                }
            }
        }
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
            FeatureCheck(deathAudioChecked, "Demo did not play the death sound on a kill");
            FeatureCheck(Audio::GetInstance()->ActiveVoiceCount() == 0, "Stop left demo audio playing");
            FeatureCheck(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard, "Editor navigation not restored");
            SetWindowPos(hwnd, nullptr, original.left, original.top, original.right-original.left,
                         original.bottom-original.top, SWP_NOZORDER);
            WriteLog(LogLevel::Info, "Smoke", "Play resize/fullscreen/minimize, managed query/visibility, navigation and detached input ownership passed");
        }
        if (frame == 59)
        {
            // Rebuild the ECS script pool in an order where Player and HUD are
            // visited before enemies. Restore/reload must not affect startup.
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
            bool spawned = false;
            for (int n = 0; n < 64; ++n) {
                auto enemy = GetWorld().FindByName("Enemy" + std::to_string(n));
                spawned |= GetWorld().GetComponent<MeshSourceComponent>(enemy).visible &&
                    GetWorld().GetComponent<TransformComponent>(enemy).transition.y > 0;
            }
            FeatureCheck(spawned, "Enemies did not spawn after multiviewport and repeated Play");
        }
        if (frame == 70 || frame == 84) mEditor.TogglePlay(*mSceneManager.GetCurrentScene());
        if (frame == 86) WriteLog(LogLevel::Info, "Smoke", "Repeated Play after multiviewport with reordered script initialization spawned enemies");
        if (mEditor.IsPlaying()) {
            auto hud=GetWorld().FindByName("HUD");
            auto text=GetWorld().GetComponent<UiElementComponent>(hud).text;
            auto at=text.find("Kills ");
            if (at != std::string::npos) {
                int kills=std::stoi(text.substr(at+6));
                if (kills>lastKills) {
                    FeatureCheck(Audio::GetInstance()->ActiveVoiceCount()>0,"Demo enemy death did not queue MP3 audio");
                    deathAudioChecked=true;
                }
                lastKills=kills;
            }
        } else lastKills=0;
        ++frame;
    }
    void Shutdown() override
    {
        AtomEngine::WindowManager::GetInstance()->SetFullscreen(false);
        EditorApp::Shutdown();
    }
};
