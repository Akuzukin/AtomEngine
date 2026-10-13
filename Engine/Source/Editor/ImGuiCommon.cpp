#include "ImGuiCommon.h"
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
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
		io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

		wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));
        mIniPath=WStringToUTF8((std::filesystem::path(exe).parent_path()/"AtomEditor.ini").wstring());
        io.IniFilename=mIniPath.c_str();
        if(std::filesystem::exists("C:/Windows/Fonts/msyh.ttc"))io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",17.f,nullptr,io.Fonts->GetGlyphRangesChineseFull());
        ImGui::StyleColorsDark();
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        // セットImGuiスタイル
		//ImGui::StyleColorsDark();

		//スケーリングの設定
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowMenuButtonPosition=ImGuiDir_None;
        style.FrameRounding = 3;
        style.GrabRounding = 3;
        style.FramePadding = ImVec2(8, 5);
        style.ItemSpacing = ImVec2(8, 6);
        style.WindowPadding = ImVec2(10, 10);
        //style.Colors[ImGuiCol_WindowBg] = ImVec4(.10f,.11f,.13f,1);
        //style.Colors[ImGuiCol_Header] = ImVec4(.18f,.26f,.36f,1);
        //style.Colors[ImGuiCol_Button] = ImVec4(.18f,.23f,.30f,1);
        //style.Colors[ImGuiCol_TabSelected] = ImVec4(.19f,.30f,.43f,1);
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
		init_info.RTVFormat = DX12Core::gBackBufferFormat;
		init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;

		init_info.SrvDescriptorHeap = Renderer::GetTextureHeap().GetHeapPointer();
		init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
			{
				auto& textureHeap = Renderer::GetTextureHeap();
				DescriptorHandle handle = textureHeap.Alloc();

				*out_cpu_handle = D3D12_CPU_DESCRIPTOR_HANDLE(handle);
				*out_gpu_handle = D3D12_GPU_DESCRIPTOR_HANDLE(handle);
			};
		init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { Renderer::GetTextureHeap().Free(DescriptorHandle(cpu_handle, gpu_handle)); };
		ImGui_ImplDX12_Init(&init_info);
	}

	void ImGuiCommon::Begin()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

	}

	void ImGuiCommon::Render(GraphicsContext& Context)
	{
		ImGui::Render();
		auto& textureHeap = Renderer::GetTextureHeap();
		Context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, textureHeap.GetHeapPointer());
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Context.GetCommandList());
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
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
}
