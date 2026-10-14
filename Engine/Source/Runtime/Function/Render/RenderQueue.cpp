#include "RenderQueue.h"
#include "RenderSystem.h"

#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/Model.h"

#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ConstantBufferStructures.h"

namespace AtomEngine
{
	void RenderQueue::AddMesh(
		const Mesh& mesh,
		const JointXform* skeleton,
		D3D12_VERTEX_BUFFER_VIEW vbv,
		D3D12_INDEX_BUFFER_VIEW ibv,
		D3D12_GPU_VIRTUAL_ADDRESS meshCBV,
		D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
		float distance,
		int subMeshIndex)
	{
		if (subMeshIndex < 0 || subMeshIndex >= (int)mesh.subMeshes.size())
			return;

		SortKey key;
		key.value = (uint64_t)mRenderObjects.size();

		bool skinned = (mesh.psoFlags & kHasSkin) == kHasSkin;
		bool MRWorkFlow = (mesh.psoFlags & kHasMRTexture) == kHasMRTexture;
		bool alphaBlend = (mesh.psoFlags & kAlphaBlend) == kAlphaBlend;
		uint64_t depthIndex = skinned ? (uint64_t)PSOIndex::kPSO_DepthOnly_Skin : (uint64_t)PSOIndex::kPSO_DepthOnly;

		union float_or_int { float f; uint32_t u; } dist;
		dist.f = std::max(distance, 0.0f);

		if (mBatchType == kShadows)
		{
			if (alphaBlend)
				return;

			key.passID = kZPass;
			key.psoIdx = skinned ? (uint64_t)PSOIndex::kPSO_Shadow_Skin : (uint64_t)PSOIndex::kPSO_Shadow;
			key.key = dist.u;
			mSortKeys.push_back(key.value);
			mTypeCounts[kZPass]++;
		}
		else if (mBatchType == kLightShadows)
		{

		}
		else if (mesh.psoFlags & kAlphaBlend)
		{
			key.passID = kTransparent;
			key.psoIdx = mesh.pso;
			key.key = static_cast<uint32_t>(~dist.u);
			mSortKeys.push_back(key.value);
			mTypeCounts[kTransparent]++;
		}
		else
		{
			// Z pass
			key.passID = kZPass;
			key.psoIdx = depthIndex;
			key.key = dist.u;
			mSortKeys.push_back(key.value);
			mTypeCounts[kZPass]++;

			// Opaque pass
			key.passID = kOpaque;
			key.psoIdx = mesh.pso;
			key.key = dist.u;
			mSortKeys.push_back(key.value);
			mTypeCounts[kOpaque]++;
		}

		RenderObject obj{};
		obj.mesh = &mesh;
		obj.skeleton = skeleton;
		obj.vbv = vbv;
		obj.ibv = ibv;
		obj.meshCBV = meshCBV;
		obj.materialCBV = materialCBV;
		obj.subMeshIndex = subMeshIndex;
		obj.srvTable = mesh.subMeshes[subMeshIndex].srvTableIndex;

		mRenderObjects.push_back(obj);
	}


	void RenderQueue::Sort()
	{
		auto cmp = [](uint64_t a, uint64_t b)
		{
			return a < b;
		};
		std::sort(mSortKeys.begin(), mSortKeys.end(), cmp);
	}

	void RenderQueue::RenderMeshes(DrawType type, GraphicsContext& context, GlobalConstants& globals)
	{
		ASSERT(mDSV != nullptr);

		const auto& rootSig = Renderer::GetRootSignature();
		const auto& textureHeap = Renderer::GetTextureHeap();
		const auto& commonSRVs = Renderer::GetCommonTextures();

		context.SetRootSignature(rootSig);
		context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, textureHeap.GetHeapPointer());

		context.SetDescriptorTable(kCommonSRVs, mEnvironmentTextures.IsNull() ? commonSRVs : mEnvironmentTextures);

		globals.ViewProjMatrix = mCamera->GetViewProjMatrix();
		globals.CameraPos = mCamera->GetPosition();

		context.SetDynamicConstantBufferView(kCommonCBV, sizeof(GlobalConstants), &globals);

		if (mBatchType == kShadows)
		{
			context.TransitionResource(*mDSV, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
			context.ClearDepth(*mDSV);
			context.SetDepthStencilTarget(mDSV->GetDSV());

			if (mViewport.Width == 0)
			{
				mViewport.TopLeftX = 0.0f;
				mViewport.TopLeftY = 0.0f;
				mViewport.Width = (float)mDSV->GetWidth();
				mViewport.Height = (float)mDSV->GetHeight();
				mViewport.MaxDepth = 1.0f;
				mViewport.MinDepth = 0.0f;

				mScissor.left = 1;
				mScissor.right = mDSV->GetWidth() - 2;
				mScissor.top = 1;
				mScissor.bottom = mDSV->GetHeight() - 2;
			}
		}
		else
		{
			ASSERT(mDSV->GetHeight() == mRTV->GetHeight());

			if (mViewport.Width == 0)
			{
				mViewport.TopLeftX = 0.0f;
				mViewport.TopLeftY = 0.0f;
				mViewport.Width = (float)mDSV->GetWidth();
				mViewport.Height = (float)mDSV->GetHeight();
				mViewport.MaxDepth = 1.0f;
				mViewport.MinDepth = 0.0f;

				mScissor.left = 0;
				mScissor.right = mDSV->GetWidth();
				mScissor.top = 0;
				mScissor.bottom = mDSV->GetHeight();
			}
		}

		for (; mCurrentType <= type; mCurrentType = (DrawType)(mCurrentType + 1))
		{
			const uint32_t typeCount = mTypeCounts[mCurrentType];
			if (typeCount == 0)
				continue;

			if (mBatchType == kDefault)
			{
				switch (mCurrentType)
				{
				case kZPass:
					context.TransitionResource(*mDSV, D3D12_RESOURCE_STATE_DEPTH_WRITE);
					context.SetDepthStencilTarget(mDSV->GetDSV());
					break;
				case kOpaque:
					context.TransitionResource(*mDSV, D3D12_RESOURCE_STATE_DEPTH_READ);
					context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
					context.SetRenderTarget(gSceneColorBuffer.GetRTV(), mDSV->GetDSV_DepthReadOnly());
					break;
				case kTransparent:
					context.TransitionResource(*mDSV, D3D12_RESOURCE_STATE_DEPTH_READ);
                    context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
                    context.SetRenderTarget(gSceneColorBuffer.GetRTV(), mDSV->GetDSV_DepthReadOnly());
					break;
				}
			}

			context.SetViewportAndScissor(mViewport, mScissor);
			context.FlushResourceBarriers();

			const uint32_t lastDraw = mCurrentDraw + typeCount;

			while (mCurrentDraw < lastDraw)
			{
				SortKey key;
				key.value = mSortKeys[mCurrentDraw];
				const RenderObject& object = mRenderObjects[key.objectIdx];
				const Mesh* mesh = object.mesh;
				const SubMesh& subMesh = mesh->subMeshes[object.subMeshIndex];

				context.SetConstantBuffer(kMeshConstants, object.meshCBV);
				context.SetConstantBuffer(kMaterialConstants, object.materialCBV);
				context.SetDescriptorTable(kMaterialSRVs, textureHeap[object.srvTable]);

				if (mesh->numJoints > 0)
				{
					ASSERT(object.skeleton != nullptr, "Can not find joint matrix array");
					context.SetDynamicSRV(kSkinMatrices, sizeof(JointXform) * mesh->numJoints, object.skeleton + mesh->startJoint);
				}
				context.SetPipelineState(Renderer::GetPSO(key.psoIdx));

				context.SetVertexBuffer(0, object.vbv);
				context.SetIndexBuffer(object.ibv);
				context.DrawIndexedInstanced(subMesh.indexCount, 1, subMesh.indexOffset, subMesh.vertexOffset, 0);

				++mCurrentDraw;
			}
		}
		if (mBatchType == kShadows)
		{
			context.TransitionResource(*mDSV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}
}
