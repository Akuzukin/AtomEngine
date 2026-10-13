#pragma once
#include "Runtime/Platform/DirectX12/Buffer/GpuBuffer.h"
#include "Runtime/Resource/TextureRef.h"
#include "Runtime/Core/Math/Matrix4x4.h"
#include "Runtime/Resource/Mesh.h"
#include <map>
namespace AtomEngine
{
class Model;
class RenderQueue;
struct JointXform;
// Device resources and draw submission are independent of imported model data.
class ModelRenderResources
{
  public:
    ~ModelRenderResources();
    ByteAddressBuffer vertexBuffer, indexBuffer;
    std::vector<DescriptorHandle> materialDescriptors;
    std::map<uint32_t, std::vector<TextureRef>> textures;
    D3D12_CPU_DESCRIPTOR_HANDLE MaterialTextureSRV(const Model &model, uint32_t material, size_t slot) const;
    void Submit(const Model &model, RenderQueue &queue, const GpuBuffer &meshConstants,
                const GpuBuffer &materialConstants, const std::vector<Matrix4x4> &nodeTransforms,
                const JointXform *skeleton, const std::vector<Mesh> &drawMeshes) const;
};
} // namespace AtomEngine
