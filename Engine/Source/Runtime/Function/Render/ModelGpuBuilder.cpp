#include "Runtime/Function/Render/ModelRenderResources.h"
#include "ModelGpuBuilder.h"
#include "RenderSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/ModelLoader.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
#include "Runtime/Platform/DirectX12/Buffer/UploadBuffer.h"
namespace AtomEngine
{
static D3D12_CPU_DESCRIPTOR_HANDLE ResolveTextureSRV(const Material &m, const Model &model, TextureSlot slot,
                                                     uint32_t index)
{
    for (size_t i = 0; i < m.textures.size(); ++i)
        if (m.textures[i].slot == slot)
        {
            auto it = model.renderResources->textures.find(index);
            if (it != model.renderResources->textures.end() && i < it->second.size())
                return it->second[i].GetSRV();
        }
    return GetDefaultTexture(slot == kNormal ? kDefaultNormalMap : kWhiteOpaque2D);
}

void LoadMaterialTexture(Model &model, const ModelData &modelData,
                         const std::map<uint32_t, std::vector<MaterialTexture>> &textures, const std::wstring &basePath)
{
    static_assert((_alignof(MaterialConstants) & 255) == 0, "CBVs need 256 byte alignment");

    const uint32_t numMaterials = (uint32_t)modelData.materials.size();
    std::vector<uint32_t> tableOffsets(numMaterials);
    std::vector<bool> hasMRTextures(numMaterials);

    for (uint32_t matIdx = 0; matIdx < numMaterials; ++matIdx)
    {
        auto it = textures.find(matIdx);
        if (it == textures.end())
            continue;
        auto &mataTextures = it->second;
        const uint32_t numTextures = (uint32_t)mataTextures.size();

        auto &modelTextures = model.renderResources->textures[matIdx];
        modelTextures.resize(numTextures);

        for (uint32_t ti = 0; ti < numTextures; ++ti)
        {
            std::wstring originalFile = basePath + mataTextures[ti].name;
            bool optionalSRGB =
                (mataTextures[ti].slot & (TextureSlot::kBaseColor | TextureSlot::kEmissive)) ? true : false;
            modelTextures[ti] = AssetManager::LoadCovertTexture(originalFile, kWhiteOpaque2D, optionalSRGB);
        }
    }

    for (uint32_t matIdx = 0; matIdx < numMaterials; ++matIdx)
    {

        const auto &material = modelData.materials[matIdx];
        uint32_t mask = material.textureMask;
        hasMRTextures[matIdx] = (mask & kMetallicRoughness) == kMetallicRoughness;

        uint32_t numBindTextures = 7;

        auto &textureHeap = Renderer::GetTextureHeap();

        DescriptorHandle TextureHandles = textureHeap.Alloc(numBindTextures);
        model.renderResources->materialDescriptors.push_back(TextureHandles);
        uint32_t SRVDescriptorTable = textureHeap.GetOffsetOfHandle(TextureHandles);

        uint32_t DestCount = numBindTextures;
        std::vector<uint32_t> SourceCounts(DestCount, 1);

        const TextureSlot slots[] = {kBaseColor, hasMRTextures[matIdx] ? kMetallicRoughness : kMetallic,
                                     kRoughness, kNormal,
                                     kOcclusion, kEmissive,
                                     kSpecular};
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> sourceTextures;
        for (auto slot : slots)
            sourceTextures.push_back(ResolveTextureSRV(material, model, slot, matIdx));
        DX12Core::gDevice->CopyDescriptors(1, &TextureHandles, &DestCount, DestCount, sourceTextures.data(),
                                           SourceCounts.data(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        tableOffsets[matIdx] = SRVDescriptorTable;
    }
    for (uint32_t i = 0; i < model.mMeshData.size(); ++i)
    {
        auto &mesh = model.mMeshData[i];

        if (!mesh.subMeshes.empty())
            mesh.srvTable = tableOffsets[mesh.subMeshes[0].materialIndex];
        for (uint32_t j = 0; j < mesh.subMeshes.size(); ++j)
        {
            auto &subMesh = mesh.subMeshes[j];
            subMesh.srvTableIndex = tableOffsets[subMesh.materialIndex];

            if (mesh.psoFlags & kHasSkin)
            {
                if (mesh.psoFlags & kAlphaBlend)
                {
                    if (hasMRTextures[subMesh.materialIndex])
                        mesh.pso = (uint16_t)PSOIndex::kPSO_Transparent_MRWorkFlow_Skin;
                    else
                        mesh.pso = (uint16_t)PSOIndex::kPSO_Transparent_Skin;
                }
                else
                {
                    if (hasMRTextures[subMesh.materialIndex])
                        mesh.pso = (uint16_t)PSOIndex::kPSO_MRWorkFlow_Skin;
                    else
                        mesh.pso = (uint16_t)PSOIndex::kPSO_Skin;
                }
            }
            else
            {
                if (mesh.psoFlags & kAlphaBlend)
                {
                    if (hasMRTextures[subMesh.materialIndex])
                        mesh.pso = (uint16_t)PSOIndex::kPSO_Transparent_MRWorkFlow;
                    else
                        mesh.pso = (uint16_t)PSOIndex::kPSO_Transparent;
                }
                else
                {
                    if (hasMRTextures[subMesh.materialIndex])
                        mesh.pso = (uint16_t)PSOIndex::kPSO_MRWorkFlow;
                    else
                        mesh.pso = (uint16_t)PSOIndex::kPSO_Default;
                }
            }
        }
    }
}

std::shared_ptr<Model> BuildModelGpuResources(ModelData &modelData, const std::wstring &path)
{
    auto model = std::make_shared<Model>();
    model->renderResources = std::make_shared<ModelRenderResources>();
    auto geometry = std::make_shared<MeshGeometry>(MeshGeometry{modelData.vertices, modelData.indices, {}});
    model->mGeometry = geometry;
    const auto basePath = GetBasePath(path);
    const auto modelName = RemoveBasePath(path);
    model->mBoundingBox = modelData.boundingBox;
    model->mBoundingSphere = modelData.boundingSphere;

    model->mMeshData = std::move(modelData.meshes);
    model->mSceneGraph = std::move(modelData.graphNodes);
    // 頂点データをアップロード
    if (!modelData.vertices.empty())
    {
        if (modelData.skinClusterData.empty())
        {
            size_t vertexBufferSize = modelData.vertices.size() * sizeof(Vertex);

            UploadBuffer vertexUpload;
            vertexUpload.Create(L"VertexUpload", vertexBufferSize);
            memcpy(vertexUpload.Map(), modelData.vertices.data(), vertexBufferSize);
            vertexUpload.Unmap();

            model->renderResources->vertexBuffer.Create(RemoveExtension(modelName) + L"VertexBuffer",
                                                        static_cast<uint32_t>(modelData.vertices.size()),
                                                        sizeof(Vertex), vertexUpload);
        }
        else
        {
            std::vector<JointVertex> sortedJointData(modelData.vertices.size());

            for (const auto &[jointName, skinInfo] : modelData.skinClusterData)
            {
                auto it = modelData.skeleton.jointMap.find(jointName);
                if (it == modelData.skeleton.jointMap.end())
                    continue;

                int32_t jointIndex = it->second;

                for (const auto &weightInfo : skinInfo.vertexWeights)
                {
                    uint32_t vertexIdx = weightInfo.vertexIndex;
                    float weightVal = weightInfo.weight;

                    if (vertexIdx < sortedJointData.size())
                    {
                        // 既存のjointIndices/weights配列の空きスロットを探して代入
                        for (int k = 0; k < 4; ++k)
                        {
                            if (sortedJointData[vertexIdx].weights[k] == 0.0f)
                            {
                                sortedJointData[vertexIdx].jointIndices[k] = jointIndex;
                                sortedJointData[vertexIdx].weights[k] = weightVal;
                                break;
                            }
                        }
                    }
                }
            }

            for (auto &joint : sortedJointData)
            {
                float sum = 0;
                for (float weight : joint.weights)
                    sum += weight;
                if (sum > 1e-6f)
                {
                    for (float &weight : joint.weights)
                        weight /= sum;
                }
                else
                {
                    joint.weights[0] = 1;
                    joint.jointIndices[0] = 0;
                }
            }
            geometry->skin = sortedJointData;
            size_t vertexSize = sizeof(Vertex);
            size_t skinSize = sizeof(JointVertex);
            size_t totalSize = modelData.vertices.size() * (vertexSize + skinSize);

            UploadBuffer vertexUpload;
            vertexUpload.Create(L"VertexUpload", totalSize);

            uint8_t *dst = reinterpret_cast<uint8_t *>(vertexUpload.Map());
            for (size_t i = 0; i < modelData.vertices.size(); ++i)
            {
                memcpy(dst, &modelData.vertices[i], vertexSize);
                dst += vertexSize;

                memcpy(dst, &sortedJointData[i], skinSize);
                dst += skinSize;
            }

            vertexUpload.Unmap();

            model->renderResources->vertexBuffer.Create(RemoveExtension(modelName) + L"VertexBuffer",
                                                        static_cast<uint32_t>(modelData.vertices.size()),
                                                        static_cast<uint32_t>(vertexSize + skinSize), vertexUpload);
        }
    }
    // インデックスデータをアップロード
    if (!modelData.indices.empty())
    {
        size_t indexBufferSize = modelData.indices.size() * sizeof(uint32_t);

        UploadBuffer indexUpload;
        indexUpload.Create(L"IndexUpload", indexBufferSize);
        memcpy(indexUpload.Map(), modelData.indices.data(), indexBufferSize);
        indexUpload.Unmap();

        model->renderResources->indexBuffer.Create(RemoveExtension(modelName) + L"IndexBuffer",
                                                   static_cast<uint32_t>(modelData.indices.size()), sizeof(uint32_t),
                                                   indexUpload);
    }

    std::map<uint32_t, std::vector<MaterialTexture>> textures;
    for (uint32_t m = 0; m < modelData.materials.size(); ++m)
    {
        for (uint32_t t = 0; t < modelData.materials[m].textures.size(); ++t)
        {
            textures[m].push_back(modelData.materials[m].textures[t]);
        }
    }

    LoadMaterialTexture(*model, modelData, textures, basePath);

    // マテリアル定数バッファアップロード
    if (modelData.materials.size() > 0)
    {
        model->mMaterials = std::move(modelData.materials);
    }

    if (!modelData.animations.empty())
    {
        model->mAnimationData = std::move(modelData.animations);
    }

    if (!modelData.skeleton.joints.empty())
    {
        model->mSkeleton = std::move(modelData.skeleton);
        model->mNumJoints = static_cast<uint32_t>(model->mSkeleton.joints.size());
        model->mJointIBMs.resize(model->mNumJoints);

        for (const auto &jointWeight : modelData.skinClusterData)
        {
            auto it = model->mSkeleton.jointMap.find(jointWeight.first);
            if (it == model->mSkeleton.jointMap.end())
            {
                continue;
            }
            model->mJointIBMs[it->second] = jointWeight.second.inverseBindPoseMatrix;
        }
    }

    return model;
}
} // namespace AtomEngine
