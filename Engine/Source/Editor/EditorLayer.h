#pragma once
#include "EditorDocument.h"
#include "MaterialPreviewRenderer.h"
#include "Runtime/Resource/MaterialPreset.h"
#include "Runtime/Function/Framework/Component/SceneComponents.h"
#include "Runtime/Function/Scene/Scene.h"
#include "Runtime/Resource/TextureRef.h"
#include <optional>
namespace AtomEngine
{
class EditorLayer
{
  public:
    void Initialize(const std::filesystem::path &path);
    void Shutdown();
    void Update(Scene &scene);
    void SetRuntimeError(const std::string &error)
    {
        mRuntimeError = error;
    }
    bool IsPlaying() const
    {
        return mPlaying;
    }
    bool IsPaused() const
    {
        return mPlaying && mPaused && !mStep;
    }
    float RuntimeDeltaTime(float dt) const
    {
        return mStep ? 1.f / 60 : dt;
    }
    bool WantsExit() const
    {
        return mExitRequested;
    }
    void RequestClose()
    {
        mPending = Pending::Exit;
    }
    void SetViewportTexture(uint64_t texture)
    {
        mViewportTexture = texture;
    }
    void BrowseAssets(const std::filesystem::path &path)
    {
        mAssetDirectory = path;
        mRefreshAssets = true;
        mShowAssets = true;
    }
    bool ViewportHovered() const
    {
        return mViewportHovered;
    }
    bool CameraInputEnabled() const
    {
        return mViewportHovered && !mGizmoActive && !mModal && !mTextInput;
    }
    Vector2 RuntimePointerPosition() const
    {
        return mRuntimePointer;
    }
    Entity Selected() const
    {
        return mSelected;
    }
    void Select(World &world, Entity entity);
    void TogglePlay(Scene &scene);
    void TogglePause()
    {
        if (mPlaying)
            mPaused = !mPaused;
    }
    void StepFrame()
    {
        if (mPlaying && mPaused)
            mStep = true;
    }
    void SetMaximizeOnPlay(bool enabled)
    {
#ifndef RELEASE
        mMaximizeOnPlay = enabled;
#endif
    }
    bool IsPlayMaximized() const
    {
        return mPlaying && mMaximizeOnPlay;
    }
    void RenderMaterialPreview(Scene &scene);
    void RenderCameraGizmos(Scene &scene);
    std::optional<Vector3> ConsumeFocus()
    {
        auto value = mFocus;
        mFocus.reset();
        return value;
    }

  private:
    std::shared_ptr<AssetPreloadBatch> mScenePreload;
    std::filesystem::path mPreloadPath;
    std::future<int> mScriptBuild;
    float mPrefabWatchTimer = 0;
    std::map<std::filesystem::path, std::filesystem::file_time_type> mPrefabStamps;
    std::string mPreloadSnapshot;
    void ScriptCreation(World &world);
    void PollBackgroundWork(Scene &scene);
    enum class Pending
    {
        None,
        New,
        Open,
        Exit
    };
    void Run(const std::function<void()> &action);
    void Hierarchy(World &world);
    void SceneObjectInspector(Scene &scene);
    void PlayToolbar(Scene &scene);
    void PlayViewport(Scene &scene);
    void ClearThumbnails();
    void Inspector(Scene &scene);
    void Create(World &world, BuiltinMesh type);
    void Workspace(Scene &scene);
    void Assets(Scene &scene);
    void Materials(Scene &scene);
    void SelectMaterial(const std::filesystem::path &path);
    void ApplyMaterial(World &world, Entity entity, const std::filesystem::path &path, int slot = -1);
    void Viewport(Scene &scene);
    void Shortcuts(Scene &scene);
    void ConfirmPending(Scene &scene);
    void ExecutePending(Scene &scene);
    bool SaveDocument(Scene &scene, bool saveAs = false);
    void Undo(Scene &scene, bool redo = false);
    void Duplicate(World &world);
    void Focus(Scene &scene);
    void Capture(Scene &scene, const std::string &label = "Edit properties");
    void OpenScene(const std::filesystem::path &path);
    void Recent(const std::filesystem::path &path);
    void LoadPreferences();
    void SavePreferences();
    EditorDocument mDocument;
    bool mDocumentReady = false, mResetLayout = false, mShowAssets = true, mShowConsole = true, mShowShaders = false,
         mShowHelp = false;
    bool mMaximizeOnPlay = false;
    std::optional<Camera> mEditCamera;
    struct Thumbnail
    {
        TextureRef texture;
        DescriptorHandle handle;
        bool failed = false;
        std::shared_ptr<AssetPreloadBatch> pending;
        std::string path;
    };
    std::map<std::string, Thumbnail> mThumbnails;
    bool mThumbnailView = true;
    bool mPlaying = false, mPaused = false, mStep = false, mExitRequested = false, mModal = false, mTextInput = false;
    bool mGizmoActive = false, mViewportHovered = false, mLocal = true, mSnap = false;
    bool mHistoryPending = true;
    int mGizmoOperation = 1;
    float mMoveSnap = .5f, mRotateSnap = 15, mScaleSnap = .1f;
    Entity mSelected = entt::null;
    std::weak_ptr<void> mWorld;
    uint64_t mViewportTexture = 0;
    Vector2 mRuntimePointer{};
    std::optional<Vector3> mFocus;
    Pending mPending = Pending::None;
    std::filesystem::path mPendingPath, mAssetDirectory = "Asset";
    std::vector<std::filesystem::path> mRecentScenes, mAssetEntries;
    bool mRefreshAssets = true;
    std::string mAssetSelection, mModelDraft, mModelDraftId;
    TextureRef mAssetPreview;
    DescriptorHandle mAssetPreviewHandle;
    char mScenePath[1024] = "", mPrefabPath[1024] = "Asset/Prefabs/New.prefab";
    std::string mError, mRuntimeError;
    std::string mPlayXml, mPlaySelection;
    bool mShowCameraGizmos = true;
    bool mShowMaterials = true, mPreviewFromSelection = true;
    int mPreviewSlot = 0;
    float mPreviewYaw = .4f, mPreviewPitch = .25f;
    MaterialPreset mPreset;
    std::unique_ptr<MaterialPreviewRenderer> mMaterialPreview;
    char mMaterialPath[1024] = "Asset/Materials/New.mat";
};
} // namespace AtomEngine
