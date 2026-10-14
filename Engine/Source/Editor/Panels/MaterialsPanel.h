#pragma once
namespace AtomEngine
{
	class EditorLayer; class Scene; class World; struct ScriptAttachment;
	// 面板负责自己的 UI；场景命令和共享选择状态由 EditorLayer 协调。
	class MaterialsPanel
	{
	public:
		void Draw(EditorLayer& editor, Scene& scene);
	};
}
