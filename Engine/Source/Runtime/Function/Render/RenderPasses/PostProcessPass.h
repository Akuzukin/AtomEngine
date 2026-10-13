#pragma once
#include "RenderPass.h"
#include "Runtime/Function/Render/PostEffect/PostEffect.h"
#include "../GridRenderer.h"

namespace AtomEngine
{
	class PostProcessPass : public RenderPass
	{
	public:
		void Initialize() override;
		void Shutdown() override;
		void Update(GraphicsContext& Context, float deltaTime) override;
		void Render(GraphicsContext& gfxContext) override;

	private:
		//システム
		std::unique_ptr<GridRenderer> mGridRenderer;
		std::unique_ptr<PostEffect> mPostEffect;
		bool mIsMotionBlurEnabled = false;
	};
}


