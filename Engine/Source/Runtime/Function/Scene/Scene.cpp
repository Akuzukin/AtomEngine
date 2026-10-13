#include "Scene.h"
#include "SceneManager.h"
#include "Runtime/Core/LogSystem/LogSystem.h"

namespace AtomEngine
{
    Scene::Scene(std::string_view name,SceneManager& manager)
     : mName(name),mManager(manager), mIsInitialized(false) {
    }

    bool Scene::Initialize()
    {

        mIsInitialized = true;
        Log("[Info]Scene: %s is initialized\n", mName.c_str());
        return true;
    }

    void Scene::RequestPopScene()
    {
        mManager.OnPopScene();
    }

    void Scene::RequestPushScene(std::unique_ptr<Scene>&& scene)
    {
        mManager.OnPushScene(std::move(scene));
    }

    void Scene::RequestReplaceScene(std::unique_ptr<Scene>&& scene)
    {
        mManager.OnReplaceScene(std::move(scene));
    }

}
