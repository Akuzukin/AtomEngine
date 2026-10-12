#pragma once
#include "IPanel.h"

namespace AtomEngine
{
	class ViewportPanel : public IPanel
	{
	public:
		void OnAttach() override;
		void Render() override;
	};
}