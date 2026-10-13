#pragma once
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"

#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Buffer/ColorBuffer.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"

namespace AtomEngine
{
class ImGuiCommon
{
  public:
    void Initialize(HWND hwnd);
    void Begin();
    void Render(GraphicsContext &Context, ColorBuffer &display);
    static void ImageDisplayEncoded(ImTextureID texture, ImVec2 size);
    static void AddImageDisplayEncoded(ImDrawList &list, ImTextureID texture, ImVec2 low, ImVec2 high);
    void Shutdown();

  private:
    std::string mIniPath;
    ColorBuffer mUiTarget;
    RootSignature mPresentRoot;
    GraphicsPSO mPresentPipeline;

  public:
    void ShowPerformanceWindow(float deltaTime);
};
} // namespace AtomEngine
