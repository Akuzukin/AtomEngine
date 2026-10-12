#pragma once
#include "ImGuiCommon.h"
#include "Runtime/Application/GameApp.h"
#include "Panels/IPanel.h"
#include <vector>

namespace AtomEngine
{
	class IPanel;
	class EditorApp : public GameApp
	{
	public:

		void Initialize() override;
		void Update(float deltaTime) override;
		void Render() override;
		void Shutdown() override;
	private:

		std::vector<std::unique_ptr<IPanel>> mPanels;
	};
}


