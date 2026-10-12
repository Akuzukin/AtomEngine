#pragma once
#include "IPanel.h"

namespace AtomEngine
{
	class HierarchyPanel : public IPanel
	{
	public:
		void OnAttach() override;
		void Render() override;

	};
}