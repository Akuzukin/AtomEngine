#pragma once
#include "IPanel.h"

namespace AtomEngine
{
	class ConsolePanel : public IPanel
	{
	public:
		void OnAttach() override;
		void Render() override;
	};
}