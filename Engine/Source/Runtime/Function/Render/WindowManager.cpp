#include "WindowManager.h"
#include "Runtime/Function/Input/Input.h"
#include "RenderSystem.h"

#include "Runtime/Core/Utility/Utility.h"
#include "Runtime/Platform/DirectX12/Core/DescriptorHeap.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Core/CommandListManager.h"



#include <mfapi.h>



namespace AtomEngine
{
	static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return static_cast<WindowManager*>(WindowManager::GetInstance())->WindowProcedure(hWnd, msg, wParam, lParam);
	}

	WindowManager* WindowManager::GetInstance()
	{
		static WindowManager instance;
		return &instance;
	}

	void* WindowManager::GetWindow()
	{
		return static_cast<void*>(mHwnd);
	}

	void WindowManager::Initialize(WindowCreateInfo info)
	{
		// COM初期化
		ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		//Media Foundationの初期化
		ThrowIfFailed(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));

		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		WNDCLASSEXW wndClass = {};
		wndClass.cbSize = sizeof(wndClass);
		wndClass.style = CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = WndProc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = GetModuleHandle(NULL);
		wndClass.hIcon = LoadIcon(0, IDI_APPLICATION);
		wndClass.hCursor = LoadCursor(0, IDC_ARROW);
		wndClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
		wndClass.lpszMenuName = 0;
		wndClass.lpszClassName = L"MainWnd";
		wndClass.hIconSm = LoadIcon(0, IDI_APPLICATION);

		RegisterClassExW(&wndClass);

		mWindowWidth = info.width;
		mWindowHeight = info.height;

		RECT rc = { 0, 0, static_cast<LONG>(mWindowWidth), static_cast<LONG>(mWindowHeight) };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
		int width = static_cast<int>(rc.right - rc.left);
		int height = static_cast<int>(rc.bottom - rc.top);

		mHwnd = CreateWindowW(
			wndClass.lpszClassName,
			info.title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			width, height,
			nullptr,
			nullptr,
			wndClass.hInstance,
			nullptr
		);

		ASSERT(mHwnd != 0);



		ShowWindow(mHwnd, SW_SHOWDEFAULT);

	}

	void WindowManager::Shutdown()
	{
		messageHandler = {};
		onResize = {};
        additionalInputWindow = {};
		if (mHwnd) { DestroyWindow(mHwnd); mHwnd = nullptr; }
		UnregisterClassW(L"MainWnd", GetModuleHandle(nullptr));
		MFShutdown();
		CoUninitialize();
	}

	void WindowManager::CloseWindow()
	{
		PostQuitMessage(0);
	}

	bool WindowManager::ShouldClose()
	{
		MSG msg;
		bool done = false;
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}

		return done;
	}

	LRESULT WindowManager::WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_MOUSEWHEEL) Input::GetInstance()->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
		if (messageHandler && messageHandler(hWnd, msg, wParam, lParam)) return true;
		switch (msg)
		{
		case WM_SIZE:
			mWindowWidth = (UINT)(UINT64)lParam & 0xFFFF;
			mWindowHeight = (UINT)(UINT64)lParam >> 16;
			mResizePending = wParam != SIZE_MINIMIZED && mWindowWidth && mWindowHeight;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		return 0;
	}

    void WindowManager::ApplyPendingResize()
    {
        if (!mResizePending) return;
        mResizePending = false;
        OnResize(mWindowWidth, mWindowHeight);
    }
    bool WindowManager::HasInputFocus() const
    {
        HWND foreground = GetForegroundWindow();
        return foreground == mHwnd || (additionalInputWindow && additionalInputWindow(foreground));
    }
    void WindowManager::SetFullscreen(bool enabled)
    {
        if (!mHwnd || enabled == mFullscreen) return;
        if (enabled)
        {
            mWindowStyle = GetWindowLongPtr(mHwnd, GWL_STYLE);
            GetWindowPlacement(mHwnd, &mWindowPlacement);
            MONITORINFO monitor{sizeof(MONITORINFO)};
            if (!GetMonitorInfo(MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST), &monitor)) return;
            SetWindowLongPtr(mHwnd, GWL_STYLE, mWindowStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(mHwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        }
        else
        {
            SetWindowLongPtr(mHwnd, GWL_STYLE, mWindowStyle);
            SetWindowPlacement(mHwnd, &mWindowPlacement);
            SetWindowPos(mHwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        mFullscreen = enabled;
    }

	void WindowManager::OnResize(uint32_t width, uint32_t height)
	{
		if (width && height && onResize) onResize(width, height);
	}
}
