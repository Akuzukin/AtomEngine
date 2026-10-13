#include "Runtime/Function/Render/ModelRenderResources.h"
/**
 * @file MeshComponent.cpp
 * @brief メッシュコンポーネントの実装
 *
 * 3Dモデルの描画、アニメーション、マテリアル管理を担当する。
 */

#include "MeshComponent.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "imgui.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"

namespace AtomEngine
{
MeshComponent::MeshComponent(std::shared_ptr<Model> model)
{
    if (!model)
    {
        mMeshConstantsCPU.Destroy();
        mMeshConstantsGPU.Destroy();
        mBoundingSphereTransforms.clear();

        mSkeletonTransforms.reset();
    }
    else
    {
        mModel = model;
        mDrawMeshes = model->mMeshData;
        mAnimator.Initialize(model);

        uint32_t numSceneNode = (uint32_t)mModel->mSceneGraph.size();
        mMeshConstantsCPU.Create(L"Mesh Constants Upload", numSceneNode * sizeof(MeshConstants));
        mMeshConstantsGPU.Create(L"Mesh Constants GPU", numSceneNode, sizeof(MeshConstants));

        mMaterialConstantsCPU.Create(L"Material Constants Upload",
                                     (uint32_t)mModel->mMaterials.size() * sizeof(MaterialConstants));
        mMaterialConstantsGPU.Create(L"Material Constants GPU", (uint32_t)mModel->mMaterials.size(),
                                     sizeof(MaterialConstants));

        mBoundingSphereTransforms.resize(numSceneNode);
        mSkeletonTransforms = std::make_unique<JointXform[]>(mModel->mNumJoints);

        if (!model->mAnimationData.empty())
        {
            mAnimGraph = std::make_unique<GraphNode[]>(numSceneNode);
            std::memcpy(mAnimGraph.get(), model->mSceneGraph.data(), model->mSceneGraph.size() * sizeof(GraphNode));

            LoopAllAnimations();
        }
        else
        {
            mAnimGraph.reset();
        }
    }
}

MeshComponent::~MeshComponent()
{
    for (auto h : mTextureTables)
        Renderer::GetTextureHeap().Free(h);

    mMeshConstantsCPU.Destroy();
    mMeshConstantsGPU.Destroy();

    mMaterialConstantsCPU.Destroy();
    mMaterialConstantsGPU.Destroy();
}

void MeshComponent::Update(GraphicsContext &gfxContext, const TransformComponent &transform,
                           const MaterialComponent &material, float deltaTime)
{
    if (!mModel || material.mMaterials.size() != mModel->mMaterials.size())
        return;

    if (!mMeshConstantsCPU.GetResource() || !mMaterialConstantsCPU.GetResource())
        return;

    UpdateMaterialTextures(material);
    constexpr size_t kMaxStackDepth = 32;

    size_t stackIdx = 0;
    Matrix4x4 matrixStack[kMaxStackDepth];
    Matrix4x4 ParentMatrix = transform.GetMatrix() * mModelTransform.GetMatrix();

    MeshConstants *cb = (MeshConstants *)mMeshConstantsCPU.Map();
    if (!cb)
        return;

    MaterialConstants *mc = (MaterialConstants *)mMaterialConstantsCPU.Map();
    if (!mc)
    {
        mMeshConstantsCPU.Unmap();
        return;
    }

    for (uint32_t i = 0; i < mModel->mMaterials.size(); ++i)
    {
        MaterialConstants &materialCB = mc[i];

        materialCB.baseColorFactor = material.mMaterials[i].mBaseColor;
        materialCB.metallicFactor = material.mMaterials[i].mMetallic;
        materialCB.roughnessFactor = material.mMaterials[i].mRoughness;
        materialCB.normalTextureScale = material.mMaterials[i].mNormalScale;
        materialCB.emissiveFactor = material.mMaterials[i].mEmissive;
        materialCB.FresnelF0 = material.mMaterials[i].mFresnelF0;
        materialCB.uvTransform = material.mMaterials[i].uvTransform.GetMatrix();
        materialCB.uselight = material.mMaterials[i].useLight;
        materialCB.aoFactor = material.mMaterials[i].mAO;
        materialCB.textureOverrides = (material.mMaterials[i].overrideModelTextures ? 127u : 0u) |
                                      (material.mMaterials[i].packedTextureChannels << 7);
        for (size_t slot = 0; slot < 7; ++slot)
            if (!material.mMaterials[i].texturePaths[slot].empty())
                materialCB.textureOverrides |= 1u << slot;
    }
    mMaterialConstantsCPU.Unmap();

    if (mAnimGraph)
    {
        UpdateAnimation(deltaTime);
        for (uint32_t i = 0; i < mModel->mSceneGraph.size(); ++i)
        {
            GraphNode &node = mAnimGraph[i];

            if (node.staleMatrix)
            {
                node.staleMatrix = false;
                node.xform = Matrix4x4::MakeScale(node.scale) * Matrix4x4(node.rotation);
            }
        }
    }

    const GraphNode *sceneGraph = mAnimGraph ? mAnimGraph.get() : mModel->mSceneGraph.data();

    // ノードがメモリに格納される順序。再帰ではなく行列スタックを使用します。
    for (const GraphNode *Node = sceneGraph;; ++Node)
    {
        Matrix4x4 xform = Node->xform;
        if (!Node->skeletonRoot)
            xform = xform * ParentMatrix;

        // 変換を親の行列と連結し、行列リストを更新します
        {
            MeshConstants &cbv = cb[Node->matrixIdx];
            cbv.worldMatrix = xform;
            cbv.worldInverseTranspose = Math::InverseTranspose(xform);

            mBoundingSphereTransforms[Node->matrixIdx] = xform;
        }

        // 次のノードがchildrenになる場合は、親行列を新しい行列に置き換えます
        if (Node->hasChildren)
        {
            if (Node->hasSibling)
            {
                ASSERT(stackIdx < kMaxStackDepth, "Overflowed the matrix stack");
                matrixStack[stackIdx++] = ParentMatrix;
            }
            ParentMatrix = xform;
        }
        else if (!Node->hasSibling)
        {
            if (stackIdx == 0)
                break;

            ParentMatrix = matrixStack[--stackIdx];
        }
    }

    for (uint32_t i = 0; i < mModel->mNumJoints; ++i)
    {
        Joint &joint = mAnimator.GetSkeleton().joints[i];
        joint.localMatrix = joint.transform.GetMatrix();
        if (joint.parent)
        {
            joint.skeletonSpaceMatrix =
                joint.localMatrix * mAnimator.GetSkeleton().joints[*joint.parent].skeletonSpaceMatrix;
        }
        else
        {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }

        JointXform &jointTrans = mSkeletonTransforms[i];
        jointTrans.posXform = mModel->mJointIBMs[i] * mAnimator.GetSkeleton().joints[i].skeletonSpaceMatrix;
        jointTrans.nrmXform = Math::InverseTranspose(jointTrans.posXform);
    }

    mMeshConstantsCPU.Unmap();

    if (!mMeshConstantsGPU.GetResource() || !mMaterialConstantsGPU.GetResource())
        return;

    gfxContext.TransitionResource(mMeshConstantsGPU, D3D12_RESOURCE_STATE_COPY_DEST, true);
    gfxContext.TransitionResource(mMaterialConstantsGPU, D3D12_RESOURCE_STATE_COPY_DEST, true);
    gfxContext.GetCommandList()->CopyBufferRegion(mMeshConstantsGPU.GetResource(), 0, mMeshConstantsCPU.GetResource(),
                                                  0, mMeshConstantsCPU.GetBufferSize());
    gfxContext.GetCommandList()->CopyBufferRegion(mMaterialConstantsGPU.GetResource(), 0,
                                                  mMaterialConstantsCPU.GetResource(), 0,
                                                  mMaterialConstantsCPU.GetBufferSize());
    gfxContext.TransitionResource(mMeshConstantsGPU, D3D12_RESOURCE_STATE_GENERIC_READ);
    gfxContext.TransitionResource(mMaterialConstantsGPU, D3D12_RESOURCE_STATE_GENERIC_READ);
}

void MeshComponent::Render(RenderQueue &sorter)
{
    if (mModel != nullptr)
    {
        mModel->renderResources->Submit(*mModel, sorter, mMeshConstantsGPU, mMaterialConstantsGPU,
                                        mBoundingSphereTransforms, mSkeletonTransforms.get(), mDrawMeshes);
    }
}
void MeshComponent::UpdateMaterialTextures(const MaterialComponent &material)
{
    std::string signature;
    bool override = false;
    for (auto &m : material.mMaterials)
    {
        signature.push_back(m.overrideModelTextures ? '1' : '0');
        override |= m.overrideModelTextures;
        for (auto &path : m.texturePaths)
        {
            signature += path;
            signature.push_back('\0');
            override |= !path.empty();
        }
    }
    if (signature == mTextureSignature)
        return;
    auto &heap = Renderer::GetTextureHeap();
    for (auto h : mTextureTables)
        heap.Free(h);
    mTextureTables.clear();
    mInstanceTextures.clear();
    mDrawMeshes = mModel->mMeshData;
    if (override)
    {
        for (size_t i = 0; i < material.mMaterials.size(); ++i)
        {
            auto h = heap.Alloc(7);
            mTextureTables.push_back(h);
            for (size_t slot = 0; slot < 7; ++slot)
            {
                D3D12_CPU_DESCRIPTOR_HANDLE srv =
                    material.mMaterials[i].overrideModelTextures
                        ? GetDefaultTexture(slot == 3 ? kDefaultNormalMap : kWhiteOpaque2D)
                        : mModel->renderResources->MaterialTextureSRV(*mModel, uint32_t(i), slot);
                auto &path = material.mMaterials[i].texturePaths[slot];
                if (!path.empty())
                {
                    mInstanceTextures.push_back(AssetManager::LoadCovertTexture(
                        path, slot == 3 ? kDefaultNormalMap : kWhiteOpaque2D, slot == 0 || slot == 5));
                    srv = mInstanceTextures.back().GetSRV();
                }
                DX12Core::gDevice->CopyDescriptorsSimple(1, h + int(slot * heap.GetDescriptorSize()), srv,
                                                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
        }
        for (auto &mesh : mDrawMeshes)
            for (auto &sub : mesh.subMeshes)
                sub.srvTableIndex = heap.GetOffsetOfHandle(mTextureTables.at(sub.materialIndex));
    }
    mTextureSignature = std::move(signature);
}
Vector3 MeshComponent::PickVertex(size_t index, uint32_t startJoint) const
{
    auto &geometry = *mModel->mGeometry;
    auto p = geometry.vertices.at(index).position;
    if (!mSkeletonTransforms || index >= geometry.skin.size())
        return p;
    auto &skin = geometry.skin[index];
    Vector3 result = Vector3::ZERO;
    for (size_t n = 0; n < 4; ++n)
    {
        auto joint = startJoint + skin.jointIndices[n];
        if (joint < mModel->mNumJoints && skin.weights[n] > 0)
            result += Math::TransformCoord(p, mSkeletonTransforms[joint].posXform) * skin.weights[n];
    }
    return result;
}
void MeshComponent::DrawSelection(GraphicsContext &ctx) const
{
    if (!mModel || !mIsRender)
        return;
    ctx.SetVertexBuffer(0, mModel->renderResources->vertexBuffer.VertexBufferView());
    ctx.SetIndexBuffer(mModel->renderResources->indexBuffer.IndexBufferView());
    for (auto &mesh : mModel->mMeshData)
    {
        if (mesh.numJoints)
            ctx.SetDynamicSRV(kSkinMatrices, sizeof(JointXform) * mesh.numJoints,
                              mSkeletonTransforms.get() + mesh.startJoint);
        for (auto &sub : mesh.subMeshes)
        {
            ctx.SetConstantBuffer(kMeshConstants,
                                  mMeshConstantsGPU.GetGpuVirtualAddress() + sub.meshCbvIndex * sizeof(MeshConstants));
            ctx.DrawIndexed(sub.indexCount, sub.indexOffset, sub.vertexOffset);
        }
    }
}
void MeshComponent::UpdateAnimation(float dt)
{
    mAnimator.UpdateAnimation(dt);
}
void MeshComponent::PlayAnimation(uint32_t i, bool loop)
{
    mAnimator.PlayAnimation(i, loop);
}
void MeshComponent::PauseAnimation(uint32_t i)
{
    mAnimator.PauseAnimation(i);
}
void MeshComponent::ResetAnimation(uint32_t i)
{
    mAnimator.ResetAnimation(i);
}
void MeshComponent::StopAnimation(uint32_t i)
{
    mAnimator.StopAnimation(i);
}
void MeshComponent::LoopAllAnimations()
{
    mAnimator.LoopAllAnimations();
}

} // namespace AtomEngine
