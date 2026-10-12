#pragma once
#include "ImGuiCommon.h"
#include <vector>

namespace AtomEngine
{
	class IPanel;
	class EditorApp
	{
	public:

		bool Initialize();
		void Update(float deltaTime);
		void Render();
		void Shutdown();
	private:

		std::vector<std::unique_ptr<IPanel>> mPanels;
	};
}


