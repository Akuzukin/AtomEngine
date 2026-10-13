#pragma once
#include"imgui.h"
#include"imgui_impl_dx12.h"
#include"imgui_impl_win32.h"
#include"ImGuizmo.h"

#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"

namespace AtomEngine
{
	class ImGuiCommon
	{
	public:

		void Initialize(HWND hwnd);
		void Begin();
		void Render(GraphicsContext& Context);
		void Shutdown();

	private:
        std::string mIniPath;
	public:

		void ShowPerformanceWindow(float deltaTime);

	};
}


