#include "ImGuiCommon.h"
#include "EditorTheme.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Core/CommandListManager.h"

namespace AtomEngine
{
	void ImGuiCommon::Initialize(HWND hwnd)
	{
		ImGui_ImplWin32_EnableDpiAwareness();
		float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
		io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

		wchar_t exe[32768]{};
		GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
		mIniPath = WStringToUTF8((std::filesystem::path(exe).parent_path() / "AtomEditor.ini").wstring());
		io.IniFilename = mIniPath.c_str();
		if (std::filesystem::exists("C:/Windows/Fonts/msyh.ttc"))
			io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 17.f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
		EditorTheme::Apply();
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(mainScale);
		style.FontScaleDpi = mainScale;
		io.ConfigDpiScaleFonts = true;
		io.ConfigDpiScaleViewports = true;

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplWin32_Init(hwnd);

		ImGui_ImplDX12_InitInfo init_info = {};
		init_info.Device = DX12Core::gDevice.Get();
		init_info.CommandQueue = DX12Core::gCommandManager.GetGraphicsQueue().GetCommandQueue();
		init_info.NumFramesInFlight = 3;
		init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;

		init_info.SrvDescriptorHeap = Renderer::GetTextureHeap().GetHeapPointer();
		init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
			D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
			{
				auto& textureHeap = Renderer::GetTextureHeap();
				DescriptorHandle handle = textureHeap.Alloc();

				*out_cpu_handle = D3D12_CPU_DESCRIPTOR_HANDLE(handle);
				*out_gpu_handle = D3D12_GPU_DESCRIPTOR_HANDLE(handle);
			};
		init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
			{
				Renderer::GetTextureHeap().Free(DescriptorHandle(cpu_handle, gpu_handle));
			};
		mPresentRoot.Reset(1);
		mPresentRoot[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		mPresentRoot.Finalize(L"ImGui SDR presentation");
		mPresentPipeline.SetRootSignature(mPresentRoot);
		mPresentPipeline.SetRasterizerState(RasterizerTwoSided);
		mPresentPipeline.SetBlendState(BlendDisable);
		mPresentPipeline.SetDepthStencilState(DepthStateDisabled);
		mPresentPipeline.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mPresentPipeline.SetRenderTargetFormat(DX12Core::gBackBufferFormat, DXGI_FORMAT_UNKNOWN);
		auto vs = ShaderCompiler::CompileBlob(L"FullScreenQuad.hlsl", L"vs_6_2"),
			ps = ShaderCompiler::CompileBlob(L"ImGuiPresentPS.hlsl", L"ps_6_2");
		mPresentPipeline.SetVertexShader(vs.Get());
		mPresentPipeline.SetPixelShader(ps.Get());
		mPresentPipeline.Finalize();
		ImGui_ImplDX12_Init(&init_info);
	}

	void ImGuiCommon::Begin()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiCommon::Render(GraphicsContext& context, ColorBuffer& display)
	{
		ImGui::Render();
		if (mUiTarget.GetWidth() != display.GetWidth() || mUiTarget.GetHeight() != display.GetHeight())
		{
			DX12Core::gCommandManager.IdleGPU();
			mUiTarget.SetClearColor(Color(0, 0, 0, 0));
			mUiTarget.Create(L"ImGui linear blend / sRGB storage", display.GetWidth(), display.GetHeight(), 1,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		}
		context.TransitionResource(mUiTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		context.ClearColor(mUiTarget);
		context.SetRenderTarget(mUiTarget.GetRTV());
		auto& heap = Renderer::GetTextureHeap();
		context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, heap.GetHeapPointer());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.GetCommandList());
		context.TransitionResource(mUiTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context.TransitionResource(display, D3D12_RESOURCE_STATE_RENDER_TARGET);
		context.SetRenderTarget(display.GetRTV());
		context.SetViewportAndScissor(0, 0, display.GetWidth(), display.GetHeight());
		context.SetRootSignature(mPresentRoot);
		context.SetPipelineState(mPresentPipeline);
		context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context.SetDynamicDescriptor(0, 0, mUiTarget.GetSRV());
		context.Draw(3);
	}
	void ImGuiCommon::ImageDisplayEncoded(ImTextureID texture, ImVec2 size)
	{
		auto* draw = ImGui::GetWindowDrawList();
		draw->AddCallback(ImGui_ImplDX12_SetTextureSRGBEncoded, nullptr);
		ImGui::Image(texture, size);
		draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}
	void ImGuiCommon::AddImageDisplayEncoded(ImDrawList& draw, ImTextureID texture, ImVec2 low, ImVec2 high)
	{
		draw.AddCallback(ImGui_ImplDX12_SetTextureSRGBEncoded, nullptr);
		draw.AddImage(texture, low, high);
		draw.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}

	void ImGuiCommon::ShowPerformanceWindow(float deltaTime)
	{
		static float frameRates[1000] = {};
		static int frameIndex = 0;
		const float frameRate = ImGui::GetIO().Framerate;
		frameRates[frameIndex] = ImGui::GetIO().Framerate;
		frameIndex = (frameIndex + 1) % IM_ARRAYSIZE(frameRates);

		ImGui::Begin("Performance");
		ImGui::Text("FPS: %.2f", frameRate);
		ImGui::Text("DeltaTime: %f", deltaTime);
		ImGui::Text("Frame Time: %.3f ms", 1000.0f / frameRate);
		ImGui::PlotLines("Frame Rate", frameRates, IM_ARRAYSIZE(frameRates), 0, nullptr, 0.0f, 144.0f, ImVec2(0, 160));
		ImGui::End();
	}

	void ImGuiCommon::Shutdown()
	{
		mUiTarget.Destroy();
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
} // namespace AtomEngine
