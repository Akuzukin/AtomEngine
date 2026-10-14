#pragma once
#include "Runtime/Function/Scene/ProjectSettings.h"
namespace AtomEngine
{
	class EditorLayer; class Scene;
	class ProjectSettingsPanel
	{
	public:
		bool visible = false;
		void Draw(EditorLayer& editor, Scene& scene);
        void Reload() { loaded = false; }
	private:
		bool loaded = false;
		int category = 0;
		ProjectSettings draft;
		std::string error;
	};
}
