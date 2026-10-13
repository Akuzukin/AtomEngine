#pragma once
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include <string>
#include <filesystem>

namespace AtomEngine
{
	class SceneManager;
	class Scene
	{
	public:
		Scene(std::string_view name,SceneManager& manager);
		virtual ~Scene() = default;
		Scene(const Scene&) = delete;
		Scene& operator=(const Scene&) = delete;

	public:

		virtual bool Initialize();
		virtual void Update(float deltaTime) = 0;
		virtual void Render() {};
		virtual void Shutdown() {};
		virtual bool Exit() { return false; }

		//シーンポップを要求
		void RequestPopScene();
		//シーンプッシュを要求
		void RequestPushScene(std::unique_ptr<Scene>&& scene);
		//シーンリプレースを要求
		void RequestReplaceScene(std::unique_ptr<Scene>&& scene);

		void Save(const std::filesystem::path& path);
        void Load(const std::filesystem::path& path);
        const std::string& GetName()const{return mName;}
        void SetName(std::string name){mName=std::move(name);}
		World& GetWorld() { return mWorld; }
		Camera& GetCamera() { return *mCamera; }
		bool IsInitialized() const { return mIsInitialized; }

	protected:
		std::string mName;
		World mWorld;
		Camera* mCamera = nullptr;
		SceneManager& mManager;

		bool mIsInitialized;

	};
}


