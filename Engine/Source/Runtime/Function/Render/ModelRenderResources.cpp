#include "ModelRenderResources.h"
#include "RenderSystem.h"
#include "RenderQueue.h"
#include "Runtime/Resource/Model.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"
namespace AtomEngine
{
D3D12_CPU_DESCRIPTOR_HANDLE ModelRenderResources::MaterialTextureSRV(const Model &model, uint32_t index,
                                                                     size_t slot) const
{
    const auto &material = model.mMaterials.at(index);
    const TextureSlot slots[] = {
        kBaseColor, (material.textureMask & kMetallicRoughness) ? kMetallicRoughness : kMetallic,
        kRoughness, kNormal,
        kOcclusion, kEmissive,
        kSpecular};
    auto refs = textures.find(index);
    for (size_t i = 0; i < material.textures.size(); ++i)
        if (material.textures[i].slot == slots[slot] && refs != textures.end() && i < refs->second.size())
            return refs->second[i].GetSRV();
    return GetDefaultTexture(slot == 3 ? kDefaultNormalMap : kWhiteOpaque2D);
}
ModelRenderResources::~ModelRenderResources()
{
    for (auto h : materialDescriptors)
        Renderer::GetTextureHeap().Free(h);
}
void ModelRenderResources::Submit(const Model &model, RenderQueue &sorter, const GpuBuffer &meshConstants,
                                  const GpuBuffer &materialConstants, const std::vector<Matrix4x4> &sphereTransforms,
                                  const JointXform *skeleton, const std::vector<Mesh> &drawMeshes) const
{
    const Frustum &frustum = sorter.GetViewFrustum();
    const Matrix4x4 &viewMat = sorter.GetViewMatrix();

    for (size_t meshIdx = 0; meshIdx < model.mMeshData.size(); ++meshIdx)
    {
        const Mesh &mesh = drawMeshes.at(meshIdx);
        for (uint32_t subIdx = 0; subIdx < mesh.subMeshes.size(); ++subIdx)
        {
            const SubMesh &sub = mesh.subMeshes[subIdx];

            const Matrix4x4 &sphereXform = sphereTransforms[sub.meshCbvIndex];
            float scaleXSqr = sphereXform.GetX().LengthSqr();
            float scaleYSqr = sphereXform.GetY().LengthSqr();
            float scaleZSqr = sphereXform.GetZ().LengthSqr();
            float sphereScale = Math::Sqrt(Math::Max(Math::Max(scaleXSqr, scaleYSqr), scaleZSqr));
            auto translate = sphereXform.GetTrans();

            BoundingSphere sphereLS = model.mBoundingSphere;
            BoundingSphere sphereWS =
                BoundingSphere(sphereLS.GetCenter() * sphereXform, sphereScale * sphereLS.GetRadius());
            BoundingSphere sphereVS = BoundingSphere(sphereWS.GetCenter() * viewMat, sphereWS.GetRadius());

            // TODO: ReverseZ 対応
            if (!frustum.IntersectSphere(sphereVS))
                continue;

            float distance = sphereVS.GetCenter().z + sphereVS.GetRadius();

            D3D12_GPU_VIRTUAL_ADDRESS meshCBV =
                meshConstants.GetGpuVirtualAddress() + sub.meshCbvIndex * sizeof(MeshConstants);

            D3D12_GPU_VIRTUAL_ADDRESS materialCBV =
                materialConstants.GetGpuVirtualAddress() + sub.materialIndex * sizeof(MaterialConstants);

            sorter.AddMesh(mesh, skeleton, vertexBuffer.VertexBufferView(), indexBuffer.IndexBufferView(), meshCBV,
                           materialCBV, distance, subIdx);
        }
    }
}

} // namespace AtomEngine
