#pragma once
#include <string>
namespace AtomEngine
{
	class EditorLayer; class Scene; class World; struct ScriptAttachment;
	// 面板负责自己的 UI；场景命令和共享选择状态由 EditorLayer 协调。
	class ScriptsPanel
	{
	public:
		void Draw(EditorLayer& editor, World& world);
		void DrawFields(EditorLayer& editor, World& world, ScriptAttachment& s, const std::string& live);
	};
}
