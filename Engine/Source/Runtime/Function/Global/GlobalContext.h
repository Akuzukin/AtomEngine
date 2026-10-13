#pragma once
#include <functional>

namespace AtomEngine
{
	class GraphicsContext;
    class ColorBuffer;
	class WindowManager;
	class Input;
	class Audio;
	class GlobalContext
	{
	public:
		// 初期化
		void Initialize();
		// 終了
		void Shutdown();
	public:
		/// グローバルコンテキストのコンポーネント
		WindowManager* windowManager = nullptr;
		Input* input = nullptr;
		Audio* audio = nullptr;
		std::function<void(GraphicsContext&)> renderOverlay;
        std::function<void(GraphicsContext&, ColorBuffer&)> prepareEditorViewport;
        std::function<void(ColorBuffer&)> capturePresented;
        bool showRenderDiagnostics = false;
        bool showGrid = true;
        bool showColliders = true;
		std::function<void()> renderPlatformWindows;
	};

	extern GlobalContext gContext;
}


