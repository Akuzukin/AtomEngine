#include "Runtime/Function/Render/WindowManager.h"
#include "Runtime/EngineCore.h"
#include "EditorApp.h"

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd)
{

	Engine::AtomEngine engine;

	AtomEngine::EditorApp app;

	engine.Run(app);
	
	return engine.Shutdown();
}