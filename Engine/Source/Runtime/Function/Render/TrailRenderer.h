#pragma once
#include "Runtime/Core/Math/Vector4.h"
#include "Runtime/Core/Math/Vector2.h"
#include "Runtime/Core/Color/Color.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include <vector>

namespace AtomEngine
{
	class GraphicsContext;
	class World;
	class Camera;
	class TrailRenderer
	{
	public:
		void Initialize();
		void Update(World& world, const Camera& camera, float deltaTime);
		void Render(GraphicsContext& gfxContext,const Camera& camera);
	private:
		struct TrailVertex
		{
			Vector4 position;
			Color color;
			Vector2 uv;
			float age;
		};
		std::vector<TrailVertex> mVertices;
		std::vector<uint16_t> mIndices;
		RootSignature mRootSig;
		GraphicsPSO mPSO;
		float time = 0.0f;
	};

}