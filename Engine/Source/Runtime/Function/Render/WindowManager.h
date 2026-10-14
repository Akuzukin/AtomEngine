#pragma once
#define NOMINMAX

#include <Windows.h>
#include <string>
#include <functional>

namespace AtomEngine
{
	struct WindowCreateInfo
	{
		int width = 1920;
		int height = 1080;
		const wchar_t* title = L"AtomEngine";
		bool isFullscreen = false;
	};

	class WindowManager
	{
	public:
		static WindowManager* GetInstance();

	public:
		void* GetWindow();
		void Initialize(WindowCreateInfo info);
		void CloseWindow();
		void Shutdown();
		std::function<void(uint32_t, uint32_t)> onResize;
		std::function<bool(HWND, UINT, WPARAM, LPARAM)> messageHandler;
		bool ShouldClose();
        void ApplyPendingResize();
        void SetFullscreen(bool enabled);
        bool IsFullscreen() const { return mFullscreen; }
        bool HasInputFocus() const;
        std::function<bool(HWND)> additionalInputWindow;
		LRESULT WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
		const HWND GetWindowHandle() const { return mHwnd; }
	private:

		HWND mHwnd = nullptr;
        bool mResizePending = false, mFullscreen = false;
        WINDOWPLACEMENT mWindowPlacement{sizeof(WINDOWPLACEMENT)};
        LONG_PTR mWindowStyle = WS_OVERLAPPEDWINDOW;

		uint32_t mWindowWidth = 0;
		uint32_t mWindowHeight = 0;

		void OnResize(uint32_t width,uint32_t height);
	};
}
