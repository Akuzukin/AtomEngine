#include "DebugDraw.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Core/GraphicsCommon.h"

namespace AtomEngine
{

	RootSignature DebugDraw::sRootSig;
	GraphicsPSO DebugDraw::sPSO;

	std::vector<DebugDrawVertex> DebugDraw::sVertices;
	Matrix4x4 DebugDraw::sViewProj;

	void DebugDraw::Initialize()
	{
		sRootSig.Reset(1, 0);
		sRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		sRootSig.Finalize(L"DebugDraw RootSig",
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);


		D3D12_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			 D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		auto vs = ShaderCompiler::CompileBlob(L"PrimitiveVS.hlsl", L"vs_6_2");
		auto ps = ShaderCompiler::CompileBlob(L"PrimitivePS.hlsl", L"ps_6_2");

		sPSO.SetRootSignature(sRootSig);
		sPSO.SetRasterizerState(RasterizerTwoSided);
		sPSO.SetBlendState(BlendTraditional);
		sPSO.SetDepthStencilState(DepthStateReadOnly);
		sPSO.SetInputLayout(_countof(layout), layout);
		sPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
		sPSO.SetRenderTargetFormats(1, &gSceneColorBuffer.GetFormat(), gSceneDepthBuffer.GetFormat());
		sPSO.SetVertexShader(vs.Get());
		sPSO.SetPixelShader(ps.Get());
		sPSO.Finalize();

	}

	void DebugDraw::Shutdown()
	{
		sVertices.clear();
	}
	void DebugDraw::DrawLine(const Vector3& a, const Vector3& b, const Color& color, const Matrix4x4& viewProj)
	{
		AddLineInternal(a, b, color);
		sViewProj = viewProj;
	}
	void DebugDraw::DrawCube(const Vector3& center, const Vector3& size,
		const Color& color, const Matrix4x4& viewProj)
	{
		Vector3 h = size * 0.5f;

		Vector3 p[8] =
		{
			{ center.x - h.x,  center.y - h.y,  center.z - h.z }, { center.x + h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x,  center.y + h.y,  center.z - h.z }, { center.x - h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x,  center.y - h.y,  center.z + h.z }, { center.x + h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x,  center.y + h.y,  center.z + h.z }, { center.x - h.x, center.y + h.y, center.z + h.z },
		};

		constexpr uint16_t edges[][2] =
		{
			{0,1},{1,2},{2,3},{3,0},
			{4,5},{5,6},{6,7},{7,4},
			{0,4},{1,5},{2,6},{3,7},
		};

		for (auto& e : edges)
			AddLineInternal(p[e[0]], p[e[1]], color);
		sViewProj = viewProj;
	}

	void DebugDraw::DrawSphere(const Vector3& center, float radius, const Color& color, const Matrix4x4& viewProj, uint32_t slices, uint32_t stacks)
	{
		if (slices < 3) slices = 3;
		if (stacks < 2) stacks = 2;

		const float dPhi = Math::PI / stacks;
		const float dTheta = Math::TwoPI / slices;

		for (uint32_t stack = 1; stack < stacks; stack++)
		{
			float phi = stack * dPhi;

			for (uint32_t slice = 0; slice < slices; slice++)
			{
				float theta0 = dTheta * slice;
				float theta1 = dTheta * (slice + 1);

				Vector3 p0{
					center.x + radius * Math::sin(phi) * Math::cos(theta0),
					center.y + radius * Math::cos(phi),
					center.z + radius * Math::sin(phi) * Math::sin(theta0)
				};

				Vector3 p1{
					center.x + radius * Math::sin(phi) * Math::cos(theta1),
					center.y + radius * Math::cos(phi),
					center.z + radius * Math::sin(phi) * Math::sin(theta1)
				};

				AddLineInternal(p0, p1, color);
			}
		}

		for (uint32_t slice = 0; slice < slices; slice++)
		{
			float theta = dTheta * slice;

			for (uint32_t stack = 0; stack < stacks; stack++)
			{
				float phi0 = dPhi * stack;
				float phi1 = dPhi * (stack + 1);

				Vector3 p0{
					center.x + radius * Math::sin(phi0) * Math::cos(theta),
					center.y + radius * Math::cos(phi0),
					center.z + radius * Math::sin(phi0) * Math::sin(theta)
				};

				Vector3 p1{
					center.x + radius * Math::sin(phi1) * Math::cos(theta),
					center.y + radius * Math::cos(phi1),
					center.z + radius * Math::sin(phi1) * Math::sin(theta)
				};

				AddLineInternal(p0, p1, color);
			}
		}
		sViewProj = viewProj;
	}

	void DebugDraw::DrawTriangle(
		const Vector3& a,
		const Vector3& b,
		const Vector3& c,
		const Color& color,
		const Matrix4x4& viewProj)
	{
		AddLineInternal(a, b, color);
		AddLineInternal(b, c, color);
		AddLineInternal(c, a, color);

		sViewProj = viewProj;
	}

	void DebugDraw::Render(GraphicsContext& ctx)
	{
		if (sVertices.empty()) return;

		ctx.SetRootSignature(sRootSig);
		ctx.SetPipelineState(sPSO);
		ctx.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

		ctx.SetDynamicVB(0, sVertices.size(), sizeof(DebugDrawVertex), sVertices.data());

		ctx.SetDynamicConstantBufferView(0, sizeof(sViewProj), &sViewProj);

		ctx.DrawInstanced((uint32_t)sVertices.size(), 1, 0, 0);

		sVertices.clear();
	}
	void DebugDraw::AddLineInternal(const Vector3& a, const Vector3& b, const Vector4& color)
	{
		sVertices.push_back({ a, color });
		sVertices.push_back({ b, color });
	}
}
