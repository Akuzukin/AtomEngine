#pragma once
#include "Runtime/Platform/DirectX12/Buffer/UploadBuffer.h"
#include "Runtime/Platform/DirectX12/Buffer/ColorBuffer.h"
#include "Runtime/Platform/DirectX12/Buffer/DepthBuffer.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Core/Math/Frustum.h"
#include "Runtime/Core/Math/Matrix4x4.h"

struct GlobalConstants;

namespace AtomEngine
{

	class CameraBase;
	struct Mesh;
	struct JointXform;

	struct RenderObject
	{
		const Mesh* mesh = nullptr;
		const JointXform* skeleton = nullptr;

		D3D12_VERTEX_BUFFER_VIEW vbv;
		D3D12_INDEX_BUFFER_VIEW ibv;

		D3D12_GPU_VIRTUAL_ADDRESS meshCBV;
		D3D12_GPU_VIRTUAL_ADDRESS materialCBV;

		uint32_t subMeshIndex = 0;
		uint32_t srvTable = 0;
	};

	class RenderQueue
	{
	public:
		enum BatchType { kDefault,kLightShadows,kShadows };
		enum DrawType { kZPass, kOpaque, kTransparent,kNumTypes };

		RenderQueue(BatchType type)
		{
			//TODO:typeによって初期化
			mBatchType = type;
			mRenderObjects.clear();
			mSortKeys.clear();
			std::memset(mTypeCounts, 0, sizeof(mTypeCounts));
			mCurrentType = kZPass;
			mCurrentDraw = 0;
		}

		void SetCamera(const CameraBase& camera)
		{
			mCamera = &camera;
		}

		void SetViewportAndScissor(const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissor)
		{ 
			mViewport = viewport;
            mScissor = scissor;
		}

		void AddRenderTarget(ColorBuffer& RTV)
		{
			mRTV = &RTV;
		}
		void SetDepthStencilTarget(DepthBuffer& DSV) { mDSV = &DSV; }
        void SetEnvironmentTextures(const DescriptorHandle &table) { mEnvironmentTextures = table; }

		const Frustum& GetWorldFrustum() const { return  mCamera->GetWorldSpaceFrustum(); }
		const Frustum& GetViewFrustum() const { return  mCamera->GetViewSpaceFrustum(); }
		const Matrix4x4& GetViewMatrix() const { return  mCamera->GetViewMatrix(); }

		void AddMesh(
			const Mesh& mesh,
			const JointXform* skeleton,
			D3D12_VERTEX_BUFFER_VIEW vbv,
			D3D12_INDEX_BUFFER_VIEW ibv,
			D3D12_GPU_VIRTUAL_ADDRESS meshCBV,
			D3D12_GPU_VIRTUAL_ADDRESS materialCBV,
			float distance,
			int subMeshIndex
		);

		void Sort();

		void RenderMeshes(DrawType type, GraphicsContext& context, GlobalConstants& globals);
	private:

		struct SortKey
		{
			union
			{
				uint64_t value;
				struct
				{
					uint64_t objectIdx : 16;
					uint64_t psoIdx : 12;
					uint64_t key : 32;
					uint64_t passID : 4;
				};
			};
		};

		std::vector<RenderObject> mRenderObjects;
		std::vector<uint64_t> mSortKeys;
		BatchType mBatchType;
		uint32_t mTypeCounts[kNumTypes];
		DrawType mCurrentType;
		uint32_t mCurrentDraw;

		const CameraBase* mCamera = nullptr;
		D3D12_VIEWPORT mViewport{};
		D3D12_RECT mScissor{};

		ColorBuffer* mRTV = nullptr;
		DepthBuffer* mDSV = nullptr;
        DescriptorHandle mEnvironmentTextures;
	};
}


