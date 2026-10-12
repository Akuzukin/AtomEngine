#pragma once
#include "IPanel.h"

namespace AtomEngine
{
	class InspectorPanel : public IPanel
	{
	public:
		void OnAttach() override;
		void Render() override;
	};
}