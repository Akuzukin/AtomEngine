#pragma once
#include "Scene.h"

namespace AtomEngine
{
	class SceneManager
	{
	public:

		void LoadScene(const std::filesystem::path& path);
		void Update(float deltaTime);
		void Render();
		void Shutdown();
		bool Exit();

		Scene* GetCurrentScene()const;


		//シーン状態のコールバック
		void OnPopScene();
		void OnPushScene(std::unique_ptr<Scene> scene);
		void OnReplaceScene(std::unique_ptr<Scene> scene);


		//Push シーン活動
		void PushScene(std::unique_ptr<Scene>&& scene);

	private:
		enum class SceneState
		{
			None,
			Push,
			Pop,
			Replace
		};

		std::vector<std::unique_ptr<Scene>> mScenes;
        SceneState mSceneState = SceneState::None;
		std::unique_ptr<Scene> mPendingScene;

	private:

		//シーン状態処置
		void ProcessSceneChange();
		//POP シーン活動
		void PopScene();
		//シーン切り替え
		void ReplaceScene(std::unique_ptr<Scene>&& scene);

	};
}
