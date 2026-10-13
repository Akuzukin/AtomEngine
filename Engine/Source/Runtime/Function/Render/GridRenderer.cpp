#include "GridRenderer.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"

namespace AtomEngine
{
	GridRenderer::GridRenderer()
	{
		xAxisColor = 0xCB3940FF;
		zAxisColor = 0x70A41CFF;
		majorLineColor = 0xCCCCCCFF;
		minorLineColor = 0xAAAAAAFF;
	}
	void GridRenderer::Initialize()
	{

		mGridRootSig.Reset(2, 0);
		mGridRootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
		mGridRootSig[1].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL);

		mGridRootSig.Finalize(L"Grid RootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		DXGI_FORMAT ColorFormat = gSceneColorBuffer.GetFormat();
		DXGI_FORMAT DepthFormat = gSceneDepthBuffer.GetFormat();

		auto gridVS = ShaderCompiler::CompileBlob(L"GridVS.hlsl", L"vs_6_2");
		auto gridPS = ShaderCompiler::CompileBlob(L"GridPS.hlsl", L"ps_6_2");

		mGridPSO.SetRootSignature(mGridRootSig);
		mGridPSO.SetRasterizerState(RasterizerDefault);
		mGridPSO.SetBlendState(BlendTraditional);
		mGridPSO.SetDepthStencilState(DepthStateReadWrite);
		mGridPSO.SetInputLayout(0, nullptr);
		mGridPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mGridPSO.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
		mGridPSO.SetInputLayout(0, nullptr);
		mGridPSO.SetVertexShader(gridVS.Get());
		mGridPSO.SetPixelShader(gridPS.Get());
		mGridPSO.Finalize();

	}

	void GridRenderer::Render(GraphicsContext& context, 
		const Camera* camera,
		ColorBuffer& colorBuffer,
		DepthBuffer& depthBuffer,
		const D3D12_VIEWPORT& viewport, 
		const D3D12_RECT& scissor)
	{
		__declspec(align(16)) struct GridVSCB
		{
			Matrix4x4 ProjInverse;
			Matrix4x4 ViewInverse;
		} gridVSCB;
		gridVSCB.ProjInverse = camera->GetProjMatrix().Inverse();
		gridVSCB.ViewInverse = camera->GetViewMatrix().Inverse();

		__declspec(align(16)) struct GridPSCB
		{
			Matrix4x4 ViewProj;
			Vector3 CameraPos;
			float farClip;
			Vector3 xAxisColor;
			float pad0;
			Vector3 zAxisColor;
			float pad1;
			Vector3 majorLineColor;
			float pad2;
			Vector3 minorLineColor;
		} gridPSCB;
		gridPSCB.ViewProj = camera->GetViewProjMatrix();
		gridPSCB.CameraPos = camera->GetPosition();
		gridPSCB.farClip = camera->GetFarClip();
		gridPSCB.xAxisColor = xAxisColor.ToVector3();
        gridPSCB.zAxisColor = zAxisColor.ToVector3();
        gridPSCB.majorLineColor = majorLineColor.ToVector3();
        gridPSCB.minorLineColor = minorLineColor.ToVector3();


		context.TransitionResource(colorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		context.TransitionResource(depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		context.SetRootSignature(mGridRootSig);
		context.SetPipelineState(mGridPSO);
		context.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context.SetRenderTarget(colorBuffer.GetRTV(), depthBuffer.GetDSV());
		context.SetViewportAndScissor(viewport, scissor);

		context.SetDynamicConstantBufferView(0, sizeof(gridVSCB), &gridVSCB);
		context.SetDynamicConstantBufferView(1, sizeof(gridPSCB), &gridPSCB);

		context.Draw(3);

	}
}
