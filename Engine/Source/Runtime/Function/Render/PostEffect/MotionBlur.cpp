#include "MotionBlur.h"
#include "Runtime/Platform/DirectX12/Context/ComputeContext.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
#include "Runtime/Platform/DirectX12/Pipeline/PipelineState.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Core/GraphicsCommon.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Core/Math/Matrix4x4.h"

namespace AtomEngine
{
	bool EnableMotionBlur = true;

	ComputePSO sCameraMotionBlurPrePassCS[2] = { {L"Motion Blur: Camera Motion Blur Pre-Pass CS"}, { L"Motion Blur: Camera Motion Blur Pre-Pass Linear Z CS" } };
	ComputePSO sMotionBlurPrePassCS(L"Motion Blur: Motion Blur Pre-Pass CS");
	ComputePSO sMotionBlurFinalPassCS(L"Motion Blur: Motion Blur Final Pass CS");
	GraphicsPSO sMotionBlurFinalPassPS(L"Motion Blur: Motion Blur Final Pass PS");
	ComputePSO sCameraVelocityCS[2] = { { L"Motion Blur: Camera Velocity CS" },{ L"Motion Blur: Camera Velocity Linear Z CS" } };

	void MotionBlur::Initialize()
	{
		auto motionBlurFinalPassCS = ShaderCompiler::CompileBlob(L"PostEffects/MotionBlur/MotionBlurFinalPassCS.hlsl", L"cs_6_6");
		auto motionBlurPrePassCS = ShaderCompiler::CompileBlob(L"PostEffects/MotionBlur/MotionBlurPrePassCS.hlsl", L"cs_6_6");
		auto cameraMotionBlurPrePassCS = ShaderCompiler::CompileBlob(L"PostEffects/MotionBlur/CameraMotionBlurPrePassCS.hlsl", L"cs_6_6");
		auto cameraMotionBlurPrePassLinearZCS = ShaderCompiler::CompileBlob(L"PostEffects/MotionBlur/CameraMotionBlurPrePassLinearZCS.hlsl", L"cs_6_6");

		auto cameraVelocityCS = ShaderCompiler::CompileBlob(L"PostEffects/MotionBlur/CameraVelocityCS.hlsl", L"cs_6_6");

		auto fullScreenQuadVS = ShaderCompiler::CompileBlob(L"FullScreenQuad.hlsl", L"vs_6_6");
		auto motionBlurFinalPassPS = ShaderCompiler::CompileBlob(L"PostEffects/MotionBlur/MotionBlurFinalPassPS.hlsl", L"ps_6_6");
#define CreatePSO( ObjName, Shader ) \
		ObjName.SetRootSignature(gCommonRS); \
		ObjName.SetComputeShader(Shader.Get()); \
		ObjName.Finalize();


		if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
		{
			CreatePSO(sMotionBlurFinalPassCS, motionBlurFinalPassCS);
		}
		else
		{
			sMotionBlurFinalPassPS.SetRootSignature(gCommonRS);
			sMotionBlurFinalPassPS.SetRasterizerState(RasterizerTwoSided);
			sMotionBlurFinalPassPS.SetBlendState(BlendPreMultiplied);
			sMotionBlurFinalPassPS.SetDepthStencilState(DepthStateDisabled);
			sMotionBlurFinalPassPS.SetSampleMask(0xFFFFFFFF);
			sMotionBlurFinalPassPS.SetInputLayout(0, nullptr);
			sMotionBlurFinalPassPS.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			sMotionBlurFinalPassPS.SetVertexShader(fullScreenQuadVS.Get());
			sMotionBlurFinalPassPS.SetPixelShader(motionBlurFinalPassPS.Get());
			sMotionBlurFinalPassPS.SetRenderTargetFormat(gSceneColorBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);
			sMotionBlurFinalPassPS.Finalize();

		}
		CreatePSO(sCameraMotionBlurPrePassCS[0], cameraMotionBlurPrePassCS);
		CreatePSO(sCameraMotionBlurPrePassCS[1], cameraMotionBlurPrePassLinearZCS);
		CreatePSO(sMotionBlurPrePassCS, motionBlurPrePassCS);
		CreatePSO(sCameraVelocityCS[0], cameraVelocityCS);
		CreatePSO(sCameraVelocityCS[1], cameraVelocityCS);

#undef CreatePSO
	}
	void MotionBlur::Shutdown()
	{
	}
	void MotionBlur::GenerateCameraVelocityBuffer(CommandContext& BaseContext, const Camera& camera, bool UseLinearZ)
	{
		GenerateCameraVelocityBuffer(BaseContext, camera.GetReprojectionMatrix(), camera.GetNearClip(), camera.GetFarClip(), UseLinearZ);
	}
	void MotionBlur::GenerateCameraVelocityBuffer(CommandContext& BaseContext, const Matrix4x4& reprojectionMatrix, float nearClip, float farClip, bool UseLinearZ)
	{
		ComputeContext& Context = BaseContext.GetComputeContext();

		Context.SetRootSignature(gCommonRS);

		uint32_t Width = gSceneColorBuffer.GetWidth();
		uint32_t Height = gSceneColorBuffer.GetHeight();

		float RcpHalfDimX = 2.0f / Width;
		float RcpHalfDimY = 2.0f / Height;
		float RcpZMagic = nearClip / (farClip - nearClip);

		Matrix4x4 preMult = Matrix4x4(
			Vector4(RcpHalfDimX, 0.0f, 0.0f, 0.0f),
			Vector4(0.0f, -RcpHalfDimY, 0.0f, 0.0f),
			Vector4(0.0f, 0.0f, UseLinearZ ? RcpZMagic : 1.0f, 0.0f),
			Vector4(-1.0f, 1.0f, UseLinearZ ? -RcpZMagic : 0.0f, 1.0f)
		);

		Matrix4x4 postMult = Matrix4x4(
			Vector4(1.0f / RcpHalfDimX, 0.0f, 0.0f, 0.0f),
			Vector4(0.0f, -1.0f / RcpHalfDimY, 0.0f, 0.0f),
			Vector4(0.0f, 0.0f, 1.0f, 0.0f),
			Vector4(1.0f / RcpHalfDimX, 1.0f / RcpHalfDimY, 0.0f, 1.0f));


		Matrix4x4 CurToPrevXForm = preMult * reprojectionMatrix * postMult;

		Context.SetDynamicConstantBufferView(3, sizeof(CurToPrevXForm), &CurToPrevXForm);
		Context.TransitionResource(gVelocityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		ColorBuffer& LinearDepth = gLinearDepth[DX12Core::GetFrameIndexMod2()];
		if (UseLinearZ)
			Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		else
			Context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		Context.SetPipelineState(sCameraVelocityCS[UseLinearZ ? 1 : 0]);
		Context.SetDynamicDescriptor(1, 0, UseLinearZ ? LinearDepth.GetSRV() : gSceneDepthBuffer.GetDepthSRV());
		Context.SetDynamicDescriptor(2, 0, gVelocityBuffer.GetUAV());
		Context.Dispatch2D(Width, Height);
	}
	void MotionBlur::RenderCameraBlur(CommandContext& BaseContext, const Camera& camera, bool UseLinearZ)
	{
		RenderCameraBlur(BaseContext, camera.GetReprojectionMatrix(), camera.GetNearClip(), camera.GetFarClip(), UseLinearZ);
	}
	void MotionBlur::RenderCameraBlur(CommandContext& BaseContext, const Matrix4x4& reprojectionMatrix, float nearClip, float farClip, bool UseLinearZ)
	{
		if (!EnableMotionBlur)
			return;

		ComputeContext& Context = BaseContext.GetComputeContext();

		Context.SetRootSignature(gCommonRS);

		uint32_t Width = gSceneColorBuffer.GetWidth();
		uint32_t Height = gSceneColorBuffer.GetHeight();

		float RcpHalfDimX = 2.0f / Width;
		float RcpHalfDimY = 2.0f / Height;
		float RcpZMagic = nearClip / (farClip - nearClip);

		Matrix4x4 preMult = Matrix4x4(
			Vector4(RcpHalfDimX, 0.0f, 0.0f, 0.0f),
			Vector4(0.0f, -RcpHalfDimY, 0.0f, 0.0f),
			Vector4(0.0f, 0.0f, UseLinearZ ? RcpZMagic : 1.0f, 0.0f),
			Vector4(-1.0f, 1.0f, UseLinearZ ? -RcpZMagic : 0.0f, 1.0f)
		);

		Matrix4x4 postMult = Matrix4x4(
			Vector4(1.0f / RcpHalfDimX, 0.0f, 0.0f, 0.0f),
			Vector4(0.0f, -1.0f / RcpHalfDimY, 0.0f, 0.0f),
			Vector4(0.0f, 0.0f, 1.0f, 0.0f),
			Vector4(1.0f / RcpHalfDimX, 1.0f / RcpHalfDimY, 0.0f, 1.0f));

		Matrix4x4 CurToPrevXForm = preMult * reprojectionMatrix * postMult;

		Context.SetDynamicConstantBufferView(3, sizeof(CurToPrevXForm), &CurToPrevXForm);

		ColorBuffer& LinearDepth = gLinearDepth[DX12Core::GetFrameIndexMod2()];
		if (UseLinearZ)
			Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		else
			Context.TransitionResource(gSceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		if (EnableMotionBlur)
		{
			Context.TransitionResource(gVelocityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.TransitionResource(gMotionPrepBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			Context.SetPipelineState(sCameraMotionBlurPrePassCS[UseLinearZ ? 1 : 0]);
			Context.SetDynamicDescriptor(1, 0, gSceneColorBuffer.GetSRV());
			Context.SetDynamicDescriptor(1, 1, UseLinearZ ? LinearDepth.GetSRV() : gSceneDepthBuffer.GetDepthSRV());
			Context.SetDynamicDescriptor(2, 0, gMotionPrepBuffer.GetUAV());
			Context.SetDynamicDescriptor(2, 1, gVelocityBuffer.GetUAV());
			Context.Dispatch2D(gMotionPrepBuffer.GetWidth(), gMotionPrepBuffer.GetHeight());

			if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
			{
				Context.SetPipelineState(sMotionBlurFinalPassCS);
				Context.SetConstants(0, 1.0f / Width, 1.0f / Height);

				Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				Context.TransitionResource(gVelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				Context.TransitionResource(gMotionPrepBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				Context.SetDynamicDescriptor(2, 0, gSceneColorBuffer.GetUAV());
				Context.SetDynamicDescriptor(1, 0, gVelocityBuffer.GetSRV());
				Context.SetDynamicDescriptor(1, 1, gMotionPrepBuffer.GetSRV());

				Context.Dispatch2D(Width, Height);

				Context.InsertUAVBarrier(gSceneColorBuffer);
			}
			else
			{
				GraphicsContext& GrContext = BaseContext.GetGraphicsContext();
				GrContext.SetRootSignature(gCommonRS);
				GrContext.SetPipelineState(sMotionBlurFinalPassPS);
				GrContext.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
				GrContext.TransitionResource(gVelocityBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				GrContext.TransitionResource(gMotionPrepBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				GrContext.SetDynamicDescriptor(1, 0, gVelocityBuffer.GetSRV());
				GrContext.SetDynamicDescriptor(1, 1, gMotionPrepBuffer.GetSRV());
				GrContext.SetConstants(0, 1.0f / Width, 1.0f / Height);
				GrContext.SetRenderTarget(gSceneColorBuffer.GetRTV());
				GrContext.SetViewportAndScissor(0, 0, Width, Height);
				GrContext.Draw(3);
			}
		}
		else
		{
			Context.SetPipelineState(sCameraVelocityCS[UseLinearZ ? 1 : 0]);
			Context.SetDynamicDescriptor(1, 0, UseLinearZ ? LinearDepth.GetSRV() : gSceneDepthBuffer.GetDepthSRV());
			Context.SetDynamicDescriptor(2, 0, gVelocityBuffer.GetUAV());
			Context.Dispatch2D(Width, Height);
		}
	}
	void MotionBlur::RenderObjectBlur(CommandContext& BaseContext, ColorBuffer& velocityBuffer)
	{
		if (!EnableMotionBlur)
			return;

		uint32_t Width = gSceneColorBuffer.GetWidth();
		uint32_t Height = gSceneColorBuffer.GetHeight();

		ComputeContext& Context = BaseContext.GetComputeContext();

		Context.SetRootSignature(gCommonRS);

		Context.TransitionResource(gMotionPrepBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(velocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		Context.SetDynamicDescriptor(2, 0, gMotionPrepBuffer.GetUAV());
		Context.SetDynamicDescriptor(1, 0, gSceneColorBuffer.GetSRV());
		Context.SetDynamicDescriptor(1, 1, velocityBuffer.GetSRV());

		Context.SetPipelineState(sMotionBlurPrePassCS);
		Context.Dispatch2D(gMotionPrepBuffer.GetWidth(), gMotionPrepBuffer.GetHeight());

		if (DX12Core::gTypedUAVLoadSupport_R11G11B10_FLOAT)
		{
			Context.SetPipelineState(sMotionBlurFinalPassCS);

			Context.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Context.TransitionResource(velocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			Context.TransitionResource(gMotionPrepBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			Context.SetDynamicDescriptor(2, 0, gSceneColorBuffer.GetUAV());
			Context.SetDynamicDescriptor(1, 0, velocityBuffer.GetSRV());
			Context.SetDynamicDescriptor(1, 1, gMotionPrepBuffer.GetSRV());
			Context.SetConstants(0, 1.0f / Width, 1.0f / Height);

			Context.Dispatch2D(Width, Height);

			Context.InsertUAVBarrier(gSceneColorBuffer);
		}
		else
		{
			GraphicsContext& GrContext = BaseContext.GetGraphicsContext();
			GrContext.SetRootSignature(gCommonRS);
			GrContext.SetPipelineState(sMotionBlurFinalPassPS);

			GrContext.TransitionResource(gSceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			GrContext.TransitionResource(velocityBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			GrContext.TransitionResource(gMotionPrepBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			GrContext.SetDynamicDescriptor(1, 0, velocityBuffer.GetSRV());
			GrContext.SetDynamicDescriptor(1, 1, gMotionPrepBuffer.GetSRV());
			GrContext.SetConstants(0, 1.0f / Width, 1.0f / Height);
			GrContext.SetRenderTarget(gSceneColorBuffer.GetRTV());
			GrContext.SetViewportAndScissor(0, 0, Width, Height);

			GrContext.Draw(3);
		}
	}
	void MotionBlur::SedtEnableMotionBlur(bool Enable)
	{
		EnableMotionBlur = Enable;
	}
}
