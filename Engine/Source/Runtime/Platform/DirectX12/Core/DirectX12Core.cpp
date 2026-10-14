#include "DirectX12Core.h"
#include "CommandListManager.h"

#include "Runtime/Core/Utility/Utility.h"
#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Global/GlobalContext.h"

#include "../Context/GraphicsContext.h"
#include "../Buffer/BufferManager.h"
#include "../Shader/ShaderCompiler.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

using namespace Microsoft::WRL;

namespace AtomEngine
{
namespace
{
struct RetiredResource
{
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    uint64_t graphics, compute, copy;
};
std::mutex retirementMutex;
std::vector<RetiredResource> retiredResources;
} // namespace
void RetireGpuResource(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
    if (!resource || !DX12Core::gCommandManager.GetGraphicsQueue().IsReady())
        return;
    std::lock_guard<std::mutex> lock(retirementMutex);
    auto &manager = DX12Core::gCommandManager;
    retiredResources.push_back({std::move(resource), manager.GetGraphicsQueue().IncrementFence(),
                                manager.GetComputeQueue().IncrementFence(), manager.GetCopyQueue().IncrementFence()});
}
void CollectRetiredResources()
{
    std::lock_guard<std::mutex> lock(retirementMutex);
    auto &manager = DX12Core::gCommandManager;
    std::erase_if(retiredResources, [&](const RetiredResource &r) {
        return manager.IsFenceComplete(r.graphics) && manager.IsFenceComplete(r.compute) &&
               manager.IsFenceComplete(r.copy);
    });
}
} // namespace AtomEngine

namespace AtomEngine
{
namespace DX12Core
{

ComPtr<ID3D12Device> gDevice = nullptr;
#ifdef _DEBUG
ComPtr<ID3D12InfoQueue1> debugInfoQueue;
DWORD debugMessageCookie = 0;
void CALLBACK DebugMessage(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id,
                           LPCSTR description, void *)
{
    auto level = severity == D3D12_MESSAGE_SEVERITY_CORRUPTION ? LogLevel::Critical
                 : severity == D3D12_MESSAGE_SEVERITY_ERROR    ? LogLevel::Error
                 : severity == D3D12_MESSAGE_SEVERITY_WARNING  ? LogLevel::Warning
                                                               : LogLevel::Debug;
    WriteLog(level, "D3D12", "[" + std::to_string(int(id)) + "] " + description);
}
#endif
CommandListManager gCommandManager;
ContextManager gContextManager;

DescriptorAllocator gDescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV};

bool gTypedUAVLoadSupport_R11G11B10_FLOAT = false;
bool gTypedUAVLoadSupport_R16G16B16A16_FLOAT = false;
bool gEnableHDROutput = false;

constexpr uint32_t kSwapChainBufferCount = 3;

uint32_t gNativeWidth = 0;
uint32_t gNativeHeight = 0;
uint32_t gBackBufferWidth = 1920;
uint32_t gBackBufferHeight = 1080;
DXGI_FORMAT gBackBufferFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

ComPtr<IDXGISwapChain4> gSwapChain = nullptr;
ColorBuffer gFrameBuffers[kSwapChainBufferCount];

UINT gCurrentBuffer = 0;
uint64_t gFrameIndex = 0;
eResolution NativeResolution = k1080p;
eResolution WindowResolution = k1080p;

RootSignature sPresentRS;
GraphicsPSO CompositePS(L"Core: Composite");
GraphicsPSO ScaleAndCompositePS(L"Core: ScaleAndComposite");

bool s_EnableVSync = true;

static void ResolutionToUINT(eResolution res, uint32_t &width, uint32_t &height)
{
    switch (res)
    {
    default:
    case k720p:
        width = 1280;
        height = 720;
        break;
    case k900p:
        width = 1600;
        height = 900;
        break;
    case k1080p:
        width = 1920;
        height = 1080;
        break;
    case k1440p:
        width = 2560;
        height = 1440;
        break;
    case k1800p:
        width = 3200;
        height = 1800;
        break;
    case k2160p:
        width = 3840;
        height = 2160;
        break;
    }
}
void SetNativeResolution(void)
{
    uint32_t NativeWidth, NativeHeight;

    ResolutionToUINT(eResolution((int)NativeResolution), NativeWidth, NativeHeight);

    if (gNativeWidth == NativeWidth && gNativeHeight == NativeHeight)
        return;
    Log("Changing Native Resolution to: %u x %u", NativeWidth, NativeHeight);

    gNativeWidth = NativeWidth;
    gNativeHeight = NativeHeight;

    gCommandManager.IdleGPU();

    InitializeBuffers(NativeWidth, NativeHeight);
}
void SetWindowResolution(void)
{
    static int SelectedDisplayRes = WindowResolution;
    if (SelectedDisplayRes == WindowResolution)
        return;

    SelectedDisplayRes = WindowResolution;
    ResolutionToUINT((eResolution)SelectedDisplayRes, gBackBufferWidth, gBackBufferHeight);
    Log("Changing Window Resolution to: %u x %u", gBackBufferWidth, gBackBufferHeight);

    gCommandManager.IdleGPU();

    OnResize(gBackBufferWidth, gBackBufferHeight);

    auto window = WindowManager::GetInstance();

    SetWindowPos(window->GetWindowHandle(), 0, 0, 0, gBackBufferWidth, gBackBufferHeight,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void InitSwapChain()
{
    ASSERT(gSwapChain == nullptr, "Graphics has already been initialized");

    ComPtr<IDXGIFactory4> dxgiFactory;
    ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = gBackBufferWidth;
    swapChainDesc.Height = gBackBufferHeight;
    swapChainDesc.Format = gBackBufferFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = kSwapChainBufferCount;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    auto window = WindowManager::GetInstance();

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(gCommandManager.GetCommandQueue(), window->GetWindowHandle(),
                                                      &swapChainDesc, &fsSwapChainDesc, nullptr,
                                                      reinterpret_cast<IDXGISwapChain1 **>(gSwapChain.GetAddressOf())));

#define CONDITIONALLY_ENABLE_HDR_OUTPUT 1

#if CONDITIONALLY_ENABLE_HDR_OUTPUT
    {
        IDXGISwapChain4 *swapChain = (IDXGISwapChain4 *)gSwapChain.Get();
        ComPtr<IDXGIOutput> output;
        ComPtr<IDXGIOutput6> output6;
        DXGI_OUTPUT_DESC1 outputDesc;
        UINT colorSpaceSupport;

        // Query support for ST.2084 on the display and set the color space accordingly
        if (SUCCEEDED(swapChain->GetContainingOutput(&output)) && SUCCEEDED(output.As(&output6)) &&
            SUCCEEDED(output6->GetDesc1(&outputDesc)) &&
            outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 &&
            SUCCEEDED(
                swapChain->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, &colorSpaceSupport)) &&
            (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) &&
            SUCCEEDED(swapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)))
        {
            gEnableHDROutput = true;
        }
    }
#endif

    for (uint32_t i = 0; i < kSwapChainBufferCount; ++i)
    {
        ComPtr<ID3D12Resource> frameResource;
        ThrowIfFailed(gSwapChain->GetBuffer(i, IID_PPV_ARGS(&frameResource)));
        gFrameBuffers[i].CreateFromSwapChain(L"Primary SwapChain Buffer", frameResource.Detach());
    }

    SetNativeResolution();

    sPresentRS.Reset(2, 1);
    sPresentRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
    sPresentRS[1].InitAsConstants(0, 6, D3D12_SHADER_VISIBILITY_ALL);

    sPresentRS.InitStaticSampler(0, SamplerLinearClampDesc);

    sPresentRS.Finalize(L"Present");

    auto FullScreenQuadBlob = ShaderCompiler::CompileBlob(L"FullScreenQuad.hlsl", L"vs_6_2");

    auto composite = ShaderCompiler::CompileBlob(L"CompositePS.hlsl", L"ps_6_2");
    CompositePS.SetRootSignature(sPresentRS);
    CompositePS.SetRasterizerState(RasterizerTwoSided);
    CompositePS.SetBlendState(BlendDisable);
    CompositePS.SetDepthStencilState(DepthStateDisabled);
    CompositePS.SetInputLayout(0, nullptr);
    CompositePS.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    CompositePS.SetVertexShader(FullScreenQuadBlob.Get());
    CompositePS.SetPixelShader(composite.Get());
    CompositePS.SetRenderTargetFormat(gBackBufferFormat, DXGI_FORMAT_UNKNOWN);
    CompositePS.Finalize();
    auto scaled = ShaderCompiler::CompileBlob(L"ScaleAndCompositePS.hlsl", L"ps_6_2");
    ScaleAndCompositePS = CompositePS;
    ScaleAndCompositePS.SetPixelShader(scaled.Get());
    ScaleAndCompositePS.Finalize();
}

void ShutdownSwapChain()
{
    gSwapChain->SetFullscreenState(FALSE, nullptr);
    gSwapChain.Reset();

    for (UINT i = 0; i < kSwapChainBufferCount; ++i)
        gFrameBuffers[i].Destroy();
}

void DX12Core::InitializeDx12(bool RequireDXRSupport)
{
#ifdef _DEBUG

    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }

#endif // _DEBUG

    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

    ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory)));

    // 使用するアダプタ用の変数。最初にnullptrを入れておく
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    // 良い順でアダプタをを頼む
    for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                             IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
         i++)
    {
        // アダプタの情報を取得する
        DXGI_ADAPTER_DESC3 adapterDesc{};
        ThrowIfFailed(useAdapter->GetDesc3(&adapterDesc));
        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
        {
            // ソフトウェアアダプタでなければ採用
            Log(L"Use Adapter:%s\n", adapterDesc.Description);
            break;
        }
        useAdapter = nullptr;
    }
    ASSERT(useAdapter != nullptr);

    // デバイスの生成
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
    const char *featureLevelStrings[] = {"12.2", "12.1", "12.0"};
    // 高い順で生成できるか試す
    for (size_t i = 0; i < _countof(featureLevels); i++)
    {
        // 採用したアダプターでデバイスを生成
        HRESULT hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&gDevice));
        if (SUCCEEDED(hr))
        {
            Log("feature levels is:%s", featureLevelStrings[i]);
            if (RequireDXRSupport)
            {
                D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
                HRESULT hrDXR = gDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
                if (FAILED(hrDXR) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
                {
                    Log("[ERROR]: DXR is required but not supported on this device.\n");
                    continue;
                }
                else
                {
                    Log("DXR supported: Tier %d\n", options5.RaytracingTier);
                }
            }

            break;
        }
    }

    ASSERT(gDevice != nullptr);
#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    if (SUCCEEDED(gDevice->QueryInterface(IID_PPV_ARGS(&infoQueue))))
    {
        // ヤバイエラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        // エラー時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // 警告時に止まる
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        // 抑制するメッセージのID
        D3D12_MESSAGE_ID denyIds[] = {
            // Windows11のDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE};
        // 抑制するレベル
        D3D12_MESSAGE_SEVERITY severities[] = {D3D12_MESSAGE_SEVERITY_INFO};
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        // 指定したメッセージの表示を抑制する
        infoQueue->PushStorageFilter(&filter);
        if (SUCCEEDED(gDevice.As(&debugInfoQueue)))
            ThrowIfFailed(debugInfoQueue->RegisterMessageCallback(DebugMessage, D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                                                                  nullptr, &debugMessageCookie));
    }
#endif
    D3D12_FEATURE_DATA_D3D12_OPTIONS FeatureData = {};
    if (SUCCEEDED(gDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &FeatureData, sizeof(FeatureData))))
    {
        if (FeatureData.TypedUAVLoadAdditionalFormats)
        {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT Support = {DXGI_FORMAT_R11G11B10_FLOAT, D3D12_FORMAT_SUPPORT1_NONE,
                                                         D3D12_FORMAT_SUPPORT2_NONE};

            if (SUCCEEDED(gDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
                (Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
            {
                gTypedUAVLoadSupport_R11G11B10_FLOAT = true;
            }

            Support.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

            if (SUCCEEDED(gDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &Support, sizeof(Support))) &&
                (Support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
            {
                gTypedUAVLoadSupport_R16G16B16A16_FLOAT = true;
            }
        }
    }

    gCommandManager.Create(gDevice.Get());
    InitializeCommonState();
    InitSwapChain();
}

void DX12Core::ShutdownDx12()
{
    gCommandManager.IdleGPU();

    CollectRetiredResources();
    CommandContext::DestroyAllContexts();

    PSO::DestroyAll();
    RootSignature::DestroyAll();
    DescriptorAllocator::DestroyAll();
    DestroyCommonState();
    DestroyBuffers();

    ShutdownSwapChain();
    // Contexts and buffers retire their resources during destruction. Drain
    // their new fences while the queues are still alive, before releasing the device.
    gCommandManager.IdleGPU();
    CollectRetiredResources();
    gCommandManager.Shutdown();

#ifdef _DEBUG
    if (debugInfoQueue)
    {
        // Live-object reporting deliberately emits warnings at shutdown. Keep
        // runtime warning/error breaks, but do not turn this diagnostic into an
        // unhandled exception after the engine has already shut down.
        debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
        debugInfoQueue->UnregisterMessageCallback(debugMessageCookie);
        debugInfoQueue.Reset();
    }
#endif
    gDevice.Reset();

    Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
    {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    }
}

uint32_t GetFrameIndexMod2()
{
    return (gFrameIndex % 2);
}

void DX12Core::OnResize(uint32_t width, uint32_t height)
{
    if (!width || !height || !gSwapChain || (width == gFrameBuffers[0].GetWidth() && height == gFrameBuffers[0].GetHeight()))
        return;
    gCommandManager.IdleGPU();

    gBackBufferWidth = width;
    gBackBufferHeight = height;

    for (uint32_t i = 0; i < kSwapChainBufferCount; ++i)
        gFrameBuffers[i].Destroy();

    // Destroy queues resources for fence retirement. DXGI requires all old
    // back-buffer references to be released before ResizeBuffers.
    gCommandManager.IdleGPU();
    CollectRetiredResources();
    ASSERT(gSwapChain != nullptr);
    ThrowIfFailed(gSwapChain->ResizeBuffers(kSwapChainBufferCount, width, height, gBackBufferFormat, 0));

    for (uint32_t i = 0; i < kSwapChainBufferCount; ++i)
    {
        ComPtr<ID3D12Resource> frameBuffer;
        ThrowIfFailed(gSwapChain->GetBuffer(i, IID_PPV_ARGS(&frameBuffer)));
        gFrameBuffers[i].CreateFromSwapChain(L"Primary SwapChain Buffer", frameBuffer.Detach());
    }

    gCurrentBuffer = gSwapChain->GetCurrentBackBufferIndex();

    gCommandManager.IdleGPU();

    ResizeBuffers(gNativeWidth, gNativeHeight);
}

void DX12Core::Present()
{
    // TODO:: HDR present

    GraphicsContext &Context = GraphicsContext::Begin(L"Present");
    Context.SetRootSignature(sPresentRS);
    Context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Context.SetDynamicDescriptor(0, 0, gSceneColorBuffer.GetSRV());

    bool NeedsScaling = gNativeWidth != gBackBufferWidth || gNativeHeight != gBackBufferHeight;

    // ui scale composite
    {
        Context.TransitionResource(gOverlayBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        Context.SetDynamicDescriptor(0, 1, gOverlayBuffer.GetSRV());
        Context.SetPipelineState(NeedsScaling ? ScaleAndCompositePS : CompositePS);
        Context.SetConstants(1, 0.7071f / gNativeWidth, 0.7071f / gNativeHeight);
        Context.TransitionResource(gFrameBuffers[gCurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET);
        Context.SetRenderTarget(gFrameBuffers[gCurrentBuffer].GetRTV());
        Context.SetViewportAndScissor(0, 0, gBackBufferWidth, gBackBufferHeight);
        Context.Draw(3);
    }

    if (gContext.prepareEditorViewport)
        gContext.prepareEditorViewport(Context, gFrameBuffers[gCurrentBuffer]);

    // ImGui Present
    if (gContext.renderOverlay)
        gContext.renderOverlay(Context, gFrameBuffers[gCurrentBuffer]);

    Context.TransitionResource(gFrameBuffers[gCurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

    if (gContext.renderPlatformWindows)
        gContext.renderPlatformWindows();
    Context.Finish(true);
    if (gContext.capturePresented)
        gContext.capturePresented(gFrameBuffers[gCurrentBuffer]);

    gSwapChain->Present(s_EnableVSync ? 1 : 0, 0);
    gCurrentBuffer = gSwapChain->GetCurrentBackBufferIndex();

    CollectRetiredResources();
    ++gFrameIndex;

    SetNativeResolution();
    SetWindowResolution();
}
} // namespace DX12Core
} // namespace AtomEngine
