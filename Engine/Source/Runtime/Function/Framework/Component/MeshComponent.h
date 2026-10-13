/**
 * @file MeshComponent.h
 * @brief メッシュコンポーネント
 *
 * 3Dモデルの描画、アニメーション再生、マテリアル管理を担当するコンポーネント。
 * スケルタルアニメーション、メッシュコンスタント、マテリアルコンスタントの管理を行う。
 */

#pragma once
#include "TransformComponent.h"
#include "Runtime/Resource/TextureRef.h"
#include "Runtime/Function/Animation/AnimationPlayer.h"
#include "MaterialComponent.h"
#include "Runtime/Platform/DirectX12/Buffer/GpuBuffer.h"
#include "Runtime/Platform/DirectX12/Buffer/UploadBuffer.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include <memory>

namespace AtomEngine
{
class RenderQueue;
/**
 * @class MeshComponent
 * @brief メッシュ描画・アニメーション管理コンポーネント
 *
 * 3Dモデルのレンダリング、スケルタルアニメーション、
 * メッシュとマテリアルの定数バッファ管理を担当する。
 *
 * 主な機能:
 * - モデルの描画
 * - スケルタルアニメーション再生
 * - マテリアルプロパティの管理
 * - GPUバッファの更新
 */
class MeshComponent
{
  public:
    /**
     * @brief コンストラクタ
     * @param model 使用する3Dモデル
     */
    MeshComponent(std::shared_ptr<Model> model);
    ~MeshComponent();

    /**
     * @brief コンポーネントの更新処理
     * @param gfxContext グラフィックスコンテキスト
     * @param transform トランスフォームコンポーネント
     * @param material マテリアルコンポーネント
     * @param deltaTime フレーム時間
     */
    void Update(GraphicsContext &gfxContext, const TransformComponent &transform, const MaterialComponent &material,
                float deltaTime);

    /**
     * @brief メッシュをレンダーキューに追加
     * @param sorter レンダーキュー
     */
    void Render(RenderQueue &sorter);
    void DrawSelection(GraphicsContext &context) const;
    Vector3 PickVertex(size_t index, uint32_t startJoint) const;
    const std::shared_ptr<Model> &GetModel() const
    {
        return mModel;
    }
    const std::vector<Matrix4x4> &NodeTransforms() const
    {
        return mBoundingSphereTransforms;
    }

    /**
     * @brief モデルのトランスフォームを取得
     * @return トランスフォーム参照
     */
    Transform &GetTransform()
    {
        return mModelTransform;
    }

    /**
     * @brief アニメーションを更新
     * @param deltaTime フレーム時間
     */
    void UpdateAnimation(float deltaTime);

    /**
     * @brief アニメーションを再生
     * @param animIdx アニメーションインデックス
     * @param loop ループ再生するか
     */
    void PlayAnimation(uint32_t animIdx, bool loop);

    /**
     * @brief アニメーションを一時停止
     * @param animIdx アニメーションインデックス
     */
    void PauseAnimation(uint32_t animIdx);

    /**
     * @brief アニメーションをリセット
     * @param animIdx アニメーションインデックス
     */
    void ResetAnimation(uint32_t animIdx);

    /**
     * @brief アニメーションを停止
     * @param animIdx アニメーションインデックス
     */
    void StopAnimation(uint32_t animIdx);

    /**
     * @brief 全アニメーションをループ再生
     */
    void LoopAllAnimations();

    /**
     * @brief アニメーション数を取得
     * @return アニメーション数
     */
    uint32_t GetNumAnimations() const
    {
        return mAnimator.GetNumAnimations();
    }

    /**
     * @brief レンダリング有効状態を取得
     * @return 有効ならtrue
     */
    bool IsRender() const
    {
        return mIsRender;
    }

    /**
     * @brief レンダリング有効状態を設定
     * @param isRender 有効にするか
     */
    void SetIsRender(bool isRender)
    {
        mIsRender = isRender;
    }

    /**
     * @brief アニメーション速度スケールを設定
     * @param deltaScale 速度倍率
     */
    void SetDeltaScale(float deltaScale)
    {
        mAnimator.SetDeltaScale(deltaScale);
    }

  private:
    std::vector<Mesh> mDrawMeshes;
    std::vector<DescriptorHandle> mTextureTables;
    std::vector<TextureRef> mInstanceTextures;
    std::string mTextureSignature;
    void UpdateMaterialTextures(const MaterialComponent &material);
    bool mIsRender = true; ///< レンダリング有効フラグ
    AnimationPlayer mAnimator;
    std::shared_ptr<Model> mModel = nullptr; ///< 3Dモデル
    UploadBuffer mMeshConstantsCPU;          ///< メッシュ定数バッファ（CPU側）
    ByteAddressBuffer mMeshConstantsGPU;     ///< メッシュ定数バッファ（GPU側）

    UploadBuffer mMaterialConstantsCPU;      ///< マテリアル定数バッファ（CPU側）
    ByteAddressBuffer mMaterialConstantsGPU; ///< マテリアル定数バッファ（GPU側）

    Transform mModelTransform;               ///< モデルのトランスフォーム
    std::unique_ptr<GraphNode[]> mAnimGraph; ///< アニメーショングラフ

    std::unique_ptr<JointXform[]> mSkeletonTransforms; ///< スケルトントランスフォーム配列
    std::vector<Matrix4x4> mBoundingSphereTransforms;  ///< バウンディングスフィアのトランスフォーム
};
} // namespace AtomEngine
