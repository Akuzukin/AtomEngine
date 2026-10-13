#include "EditorApp.h"
#include "External/tinyxml2/tinyxml2.h"
#include "Runtime/Core/LogSystem/LogSystem.h"
#include "Runtime/Function/Global/GlobalContext.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace AtomEngine
{
void EditorApp::Initialize()
{
    mImGui.Initialize(gContext.windowManager->GetWindowHandle());
    mViewportDescriptor = Renderer::GetTextureHeap().Alloc();
    mViewportTexture.Create(L"Editor scene preview", DX12Core::gBackBufferWidth, DX12Core::gBackBufferHeight, 1,
                            DX12Core::gBackBufferFormat);
    DX12Core::gDevice->CopyDescriptorsSimple(1, mViewportDescriptor, mViewportTexture.GetSRV(),
                                             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    mEditor.SetViewportTexture(mViewportDescriptor.GetGpuPtr());
    gContext.showColliders = false;
    gContext.prepareEditorViewport = [this](GraphicsContext &context, ColorBuffer &display) {
        if (mViewportTexture.GetWidth() != display.GetWidth() || mViewportTexture.GetHeight() != display.GetHeight())
        {
            DX12Core::gCommandManager.IdleGPU();
            mViewportTexture.Create(L"Editor scene preview", display.GetWidth(), display.GetHeight(), 1,
                                    DX12Core::gBackBufferFormat);
            DX12Core::gDevice->CopyDescriptorsSimple(1, mViewportDescriptor, mViewportTexture.GetSRV(),
                                                     D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
        context.CopyBuffer(mViewportTexture, display);
        context.TransitionResource(mViewportTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        context.TransitionResource(display, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        context.SetRenderTarget(display.GetRTV());
        context.SetViewportAndScissor(0, 0, display.GetWidth(), display.GetHeight());
        context.ClearColor(display);
    };
    gContext.renderPlatformWindows = [] {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    };
    gContext.renderOverlay = [this](GraphicsContext &context, ColorBuffer &display) {
        mImGui.Render(context, display);
    };
    gContext.windowManager->messageHandler = [this](HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        if (msg == WM_CLOSE)
        {
            mEditor.RequestClose();
            return true;
        }
        return ImGui_ImplWin32_WndProcHandler(hwnd, msg, w, l) != 0;
    };
    tinyxml2::XMLDocument settings;
    if (settings.LoadFile("Asset/Editor.xml") != tinyxml2::XML_SUCCESS)
        throw std::runtime_error("Cannot read Asset/Editor.xml");
    auto root = settings.FirstChildElement("Editor");
    auto startup = root ? root->Attribute("startupScene") : nullptr;
    if (!startup || !*startup)
        throw std::runtime_error("Editor startupScene is missing");
    mSceneManager.LoadScene(UTF8ToWString(startup));
    mSceneManager.Update(0);
    mEditor.Initialize(UTF8ToWString(startup));
    if (auto c = root->FirstChildElement("Camera"); c && !GetWorld().settings.hasEditorCamera)
    {
        GetCamera().SetLookAt(
            Vector3(c->FloatAttribute("x", 10), c->FloatAttribute("y", 10), c->FloatAttribute("z", -15)),
            Vector3(c->FloatAttribute("targetX"), c->FloatAttribute("targetY"), c->FloatAttribute("targetZ")),
            Vector3(0, 1, 0));
        GetCamera().Update();
    }
}

void EditorApp::BeginFrame()
{
    mImGui.Begin();
}

void EditorApp::Update(float deltaTime)
{
    mSceneManager.Update(deltaTime);
    if (auto scene = mSceneManager.GetCurrentScene())
    {
        if (mCameraWorld.lock() != scene->GetWorld().GetLifetime().lock())
        {
            mCameraController = std::make_unique<DebugCamera>(scene->GetCamera());
            mCameraWorld = scene->GetWorld().GetLifetime();
        }
        scene->GetCamera().Update();
        mEditor.Update(*scene);
        if (auto focus = mEditor.ConsumeFocus())
        {
            mCameraController = std::make_unique<DebugCamera>(scene->GetCamera());
            mCameraController->SetTarget(*focus);
            mCameraController->SetDistance((scene->GetCamera().GetPosition() - *focus).Length());
        }
        if (!mEditor.IsPlaying() && mEditor.CameraInputEnabled())
        {
            auto &io = ImGui::GetIO();
            DebugCameraInput input;
            input.dx = io.MouseDelta.x;
            input.dy = io.MouseDelta.y;
            input.wheel = io.MouseWheel;
            input.fly = io.KeyShift;
            input.boost = io.KeyShift;
            input.orbit = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            input.pan = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            input.look = input.fly && ImGui::IsMouseDown(ImGuiMouseButton_Right);
            input.zoom = !input.fly && ImGui::IsMouseDown(ImGuiMouseButton_Right);
            input.movement = Vector3(float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A)),
                                     float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q)),
                                     float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S)));
            mCameraController->Update(deltaTime, input);
        }
        else
            mCameraController->ResetInputTracking();
    }
}
void EditorApp::Render()
{
    if (auto scene = mSceneManager.GetCurrentScene())
    {
        mEditor.RenderMaterialPreview(*scene);
        mEditor.RenderCameraGizmos(*scene);
    }
}
void EditorApp::Shutdown()
{
    mEditor.Shutdown();
    mCameraController.reset();
    mCameraWorld.reset();
    mSceneManager.Shutdown();
    gContext.renderOverlay = {};
    gContext.prepareEditorViewport = {};
    gContext.capturePresented = {};
    Renderer::GetTextureHeap().Free(mViewportDescriptor);
    mViewportTexture.Destroy();
    gContext.showColliders = true;
    gContext.showRenderDiagnostics = false;
    gContext.renderPlatformWindows = {};
    gContext.windowManager->messageHandler = {};
    mImGui.Shutdown();
}
} // namespace AtomEngine
