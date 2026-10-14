#pragma once
namespace AtomEngine
{
	class EditorLayer; class Scene; class World; struct ScriptAttachment;
	// 面板负责自己的 UI；场景命令和共享选择状态由 EditorLayer 协调。
	class ViewportPanel
	{
	public:
		void Draw(EditorLayer& editor, Scene& s);
		void DrawPlay(EditorLayer& editor, Scene& scene);
	};
}
