#include "SceneManager.h"
#include "SceneSerializer.h"
#include "SceneView.h"
namespace AtomEngine
{
namespace
{
class FileScene final : public Scene
{
    Camera camera;
    std::filesystem::path file;

  public:
    FileScene(SceneManager &m, const std::filesystem::path &p) : Scene(p.stem().string(), m), file(p)
    {
        mCamera = &camera;
        camera.SetLookAt(Vector3(10, 10, -15), Vector3(0, 0, 0), Vector3(0, 1, 0));
        camera.Update();
    }
    bool Initialize() override
    {
        mName = SceneSerializer::Load(mWorld, file);
        RestoreEditorCamera(mWorld, camera);
        EnsureSceneViewObjects(mWorld, camera);
        return Scene::Initialize();
    }
    void Update(float) override
    {
        camera.Update();
    }
};
} // namespace
void Scene::Save(const std::filesystem::path &p)
{
    auto &s = mWorld.settings;
    s.editorPosition = mCamera->GetPosition();
    s.editorRotation = mCamera->GetRotation();
    s.editorFov = mCamera->GetFOV() * 180 / Math::PI;
    s.editorNear = mCamera->GetNearClip();
    s.editorFar = mCamera->GetFarClip();
    s.hasEditorCamera = true;
    SceneSerializer::Save(mWorld, p, mName);
}
void Scene::Load(const std::filesystem::path &p)
{
    mName = SceneSerializer::Load(mWorld, p);
    RestoreEditorCamera(mWorld, *mCamera);
    EnsureSceneViewObjects(mWorld, *mCamera);
}
void SceneManager::LoadScene(const std::filesystem::path &p)
{
    OnReplaceScene(std::make_unique<FileScene>(*this, p));
}
} // namespace AtomEngine
