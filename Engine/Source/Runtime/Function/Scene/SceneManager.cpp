#include"SceneManager.h"

namespace AtomEngine
{
	void SceneManager::Update(float deltaTime)
	{
		Scene* current = GetCurrentScene();
		if (current && current->IsInitialized())
			current->Update(deltaTime);

		ProcessSceneChange();
	}

	void SceneManager::Render()
	{
		Scene* current = GetCurrentScene();
		if (current && current->IsInitialized())
			current->Render();
	}

	void SceneManager::Shutdown()
	{
		for (auto& scene : mScenes)
			if (scene)
				scene->Shutdown();

		mScenes.clear();
		mPendingScene.reset();
		mSceneState = SceneState::None;
	}

	bool SceneManager::Exit()
	{
		Scene* current = GetCurrentScene();
		if (current && current->IsInitialized())
			return current->Exit();
		return false;
	}

	Scene* SceneManager::GetCurrentScene() const
	{
		if (mScenes.empty()) return nullptr;
		return mScenes.back().get();
	}

	void SceneManager::OnPopScene()
	{
		mSceneState = SceneState::Pop;
	}

	void SceneManager::OnPushScene(std::unique_ptr<Scene> scene)
	{
		mPendingScene = std::move(scene);
		mSceneState = SceneState::Push;
	}

	void SceneManager::OnReplaceScene(std::unique_ptr<Scene> scene)
	{
		mPendingScene = std::move(scene);
		mSceneState = SceneState::Replace;
	}

	void SceneManager::ProcessSceneChange()
	{
		if (mSceneState == SceneState::None)
			return;

		const auto command = mSceneState;
		auto pending = std::move(mPendingScene);
		mSceneState = SceneState::None;
		switch (command)
		{
		case SceneState::Push:
			PushScene(std::move(pending));
			break;

		case SceneState::Pop:
			PopScene();
			break;

		case SceneState::Replace:
			ReplaceScene(std::move(pending));
			break;
		}

	}

	void SceneManager::PushScene(std::unique_ptr<Scene>&& scene)
	{
		if (!scene)
			return;

		if (scene->Initialize())
		{
			mScenes.emplace_back(std::move(scene));
		}
	}

	void SceneManager::PopScene()
	{
		if (mScenes.empty())
			return;

		auto& top = mScenes.back();
		top->Shutdown();
		mScenes.pop_back();

	}

	void SceneManager::ReplaceScene(std::unique_ptr<Scene>&& scene)
	{
		if (!mScenes.empty())
		{
			mScenes.back()->Shutdown();
			mScenes.pop_back();
		}

		PushScene(std::move(scene));
	}
}
