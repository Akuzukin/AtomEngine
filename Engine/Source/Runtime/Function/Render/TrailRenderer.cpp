#include "TrailRenderer.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Function/Framework/Component/TrailComponent.h"
#include "Runtime/Function/Framework/Component/TransformComponent.h"
#include "Runtime/Platform/DirectX12/Core/GraphicsCommon.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Function/Camera/CameraBase.h"

namespace AtomEngine
{
	void TrailRenderer::Initialize()
	{
		mRootSig.Reset(2);
		mRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		mRootSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);
		mRootSig.Finalize(L"TrailRenderer", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        D3D12_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
             D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

		auto trailVS = ShaderCompiler::CompileBlob(L"Trail.VS.hlsl",L"vs_6_6");
        auto trailPS = ShaderCompiler::CompileBlob(L"Trail.PS.hlsl", L"ps_6_6");

		mPSO.SetRootSignature(mRootSig);
        mPSO.SetRasterizerState(RasterizerDefaultCw);
        mPSO.SetBlendState(BlendTraditionalAdditive);
        mPSO.SetDepthStencilState(DepthStateReadOnly);
        mPSO.SetInputLayout(_countof(layout), layout);
        mPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
        mPSO.SetRenderTargetFormats(1, &gSceneColorBuffer.GetFormat(), gSceneDepthBuffer.GetFormat());
        mPSO.SetVertexShader(trailVS.Get());
        mPSO.SetPixelShader(trailPS.Get());
        mPSO.Finalize();
    }

	void TrailRenderer::Update(World& world, const Camera& camera, float deltaTime)
	{
		mVertices.clear();
		mIndices.clear();
        time += deltaTime;
		auto view = world.View<TrailComponent,TransformComponent>();
		for (auto entity : view)
		{
			auto& trail = view.get<TrailComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			if (deltaTime > 0.0f)
				trail.sampleInterval = deltaTime;
			// 既存サンプルを経過時間分だけ進める
			for (auto& s : trail.samples)s.age += deltaTime;

			// サンプリングタイミング管理
			trail.timeSinceLast += deltaTime;
			if (trail.timeSinceLast >= trail.sampleInterval && trail.active)
			{
				trail.timeSinceLast = 0.0f;
				TrailComponent::Sample sample;
				sample.pos = transform.transition;
				sample.age = 0.0f;
				// newest を front に入れる
				trail.samples.push_front(sample);
				if (trail.samples.size() > trail.maxSamples)
				{
					trail.samples.pop_back();
				}
			}

			// 期限切れサンプルを削除
			while (!trail.samples.empty() && trail.samples.back().age > trail.maxAge)
			{
				trail.samples.pop_back();
			}

			if (trail.samples.size() < 2) continue;
			size_t baseIndex = mVertices.size();
			const auto& s = trail.samples;
			size_t n = s.size();

            for (size_t i = 0; i < n; ++i)
            {
                // 隣接サンプルを使って接線を計算
                Vector3 p = s[i].pos;
                Vector3 dir;
                if (i == 0) dir = (n > 1) ? (s[0].pos - s[1].pos) : Vector3(1, 0, 0);
                else if (i == n - 1) dir = (s[n - 1].pos - s[n - 2].pos);
                else dir = (s[i - 1].pos - s[i + 1].pos);

                dir = Math::Normalize(dir);
                Vector3 normal = Math::Normalize(Math::Cross(dir, camera.GetForwardVec()));

                float t = Math::Clamp(s[i].age / trail.maxAge, 0.0f, 1.0f);

                float widthFactor = Math::SmoothStep(1.0f, 0.0f, t);
                float localThickness = trail.thickness * widthFactor;

                Vector3 leftPos = p + normal * (localThickness * 0.5f);
                Vector3 rightPos = p - normal * (localThickness * 0.5f);
                Color vertexColor;
                float tNorm = widthFactor; // 0..1 (若い=0 -> ヘッド)
                if (trail.useSegmentedColors && trail.colorSegments > 0)
                {
                    int segCount = static_cast<int>(trail.colorSegments);
                    float segSize = 1.0f / static_cast<float>(segCount);
                    int segIdx = std::min(static_cast<int>(tNorm / segSize), segCount - 1);
                    float segStart = segIdx * segSize;
                    float segEnd = std::min((segIdx + 1) * segSize, 1.0f);
                    float localT = (segEnd - segStart) > 0.0f ? ((tNorm - segStart) / (segEnd - segStart)) : 0.0f;
                    Color cStart = Math::Lerp(trail.startColor, trail.endColor, segStart);
                    Color cEnd = Math::Lerp(trail.startColor, trail.endColor, segEnd);
                    vertexColor = Math::Lerp(cStart, cEnd, localT);
                }
                else
                {
                    // 連続グラデーション（フォールバック）
                    vertexColor = Math::Lerp(trail.startColor, trail.endColor, tNorm);
                }
                float age01 = Math::Clamp(s[i].age / trail.maxAge, 0.0f, 1.0f);

                TrailVertex vLeft;
                vLeft.position = Vector4(leftPos.x, leftPos.y, leftPos.z, 1.0f);
                vLeft.color = vertexColor;
                vLeft.uv = Vector2(0.0f, age01);
                vLeft.age = s[i].age;
                mVertices.push_back(vLeft);

                TrailVertex vRight;
                vRight.position = Vector4(rightPos.x, rightPos.y, rightPos.z, 1.0f);
                vRight.color = vertexColor;
                vRight.uv = Vector2(1.0f, age01);
                vRight.age = s[i].age;
                mVertices.push_back(vRight);
            }

            // インデックス（各サンプルで四角を二つの三角形に分ける）
            for (size_t i = 0; i + 1 < n; ++i)
            {
                uint16_t i0 = static_cast<uint16_t>(baseIndex + i * 2 + 0);
                uint16_t i1 = static_cast<uint16_t>(baseIndex + i * 2 + 1);
                uint16_t i2 = static_cast<uint16_t>(baseIndex + (i + 1) * 2 + 0);
                uint16_t i3 = static_cast<uint16_t>(baseIndex + (i + 1) * 2 + 1);

                // tri1: i0, i2, i1
                mIndices.push_back(i0);
                mIndices.push_back(i2);
                mIndices.push_back(i1);
                // tri2: i1, i2, i3
                mIndices.push_back(i1);
                mIndices.push_back(i2);
                mIndices.push_back(i3);
            }
		}
	}
	void TrailRenderer::Render(GraphicsContext& gfxContext,const Camera& camera)
	{
        if (mVertices.empty() || mIndices.empty()) return;
        gfxContext.SetRootSignature(mRootSig);
        gfxContext.SetPipelineState(mPSO);
        gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        gfxContext.SetDynamicConstantBufferView(0, sizeof(Matrix4x4), &camera.GetViewProjMatrix());
        // マテリアル色（透明含む）
        _declspec(align(16)) struct
        {
            float maxAge;
            float time;
            float pad[2];
        } matConst;
        matConst.maxAge = 0.6f;
        matConst.time = time;
        gfxContext.SetDynamicConstantBufferView(1, sizeof(matConst), &matConst);

        // 頂点・インデックスをバインドして描画
        gfxContext.SetDynamicVB(0, mVertices.size(), sizeof(TrailVertex), mVertices.data());
        gfxContext.SetDynamicIB(mIndices.size(), mIndices.data());

        gfxContext.DrawIndexedInstanced(static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);
	}
}