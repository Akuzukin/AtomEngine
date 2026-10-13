#pragma once
#include "RenderPass.h"

namespace AtomEngine
{
	class OverlayPass : public RenderPass
	{
	public:

		void Initialize()override;
        void Shutdown()override;
		void Render(GraphicsContext& gfxContext) override;
	};
}


