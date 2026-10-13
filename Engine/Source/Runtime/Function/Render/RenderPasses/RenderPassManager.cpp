#include "RenderPassManager.h"

namespace AtomEngine
{
	RenderPassManager* RenderPassManager::GetInstance()
	{
		static RenderPassManager instance;
		return &instance;
	}

	void RenderPassManager::RegisterRenderPass(std::unique_ptr<RenderPass> pass)
	{
		pass->Initialize();
		mAllPasses.push_back(std::move(pass));
	}

	void RenderPassManager::SetUpRenderPass()
	{
		mCurrentPasses.clear();
		for (auto& pass : mAllPasses)
			mCurrentPasses.push_back(pass.get());
	}

	void RenderPassManager::Update(GraphicsContext& context, float deltaTime)
	{
		for (auto* pass : mCurrentPasses)
			pass->Update(context, deltaTime);
	}

	void RenderPassManager::RenderAll(GraphicsContext& context)
	{
		for (auto* pass : mCurrentPasses)
			pass->Render(context);
	}

	void RenderPassManager::SetRenderTarget(ColorBuffer* rtv, DepthBuffer* dsv)
	{
		for (auto* pass : mCurrentPasses)
			pass->SetRenderTarget(rtv, dsv);
	}

	void RenderPassManager::SetDepthStencilTarget(DepthBuffer* dsv)
	{
		for (auto* pass : mCurrentPasses)
			pass->SetDepthStencilTarget(dsv);
	}

	void RenderPassManager::SetCamera(Camera* camera)
	{
		for (auto* pass : mCurrentPasses)
		{
			pass->SetCamera(camera);
		}
	}

	void RenderPassManager::SetViewportAndScissor(const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor)
	{
		for (auto* pass : mCurrentPasses)
			pass->SetViewportAndScissor(viewport, scissor);
	}

	void RenderPassManager::SetWorld(World& world)
	{
		for (auto* pass : mCurrentPasses)
			pass->SetWorld(world);
	}

	void RenderPassManager::Shutdown()
	{
		for (auto it = mAllPasses.rbegin(); it != mAllPasses.rend(); ++it) (*it)->Shutdown();
		mAllPasses.clear();
		mCurrentPasses.clear();
	}

}

