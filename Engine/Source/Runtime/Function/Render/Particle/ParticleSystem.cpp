#include "ParticleSystem.h"
#include "Runtime/Platform/DirectX12/Buffer/ReadbackBuffer.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Utility/Utility.h"

#include "imgui.h"
#include <json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <mutex>

namespace AtomEngine
{
	using json = nlohmann::json;

	RootSignature ParticleSystem::mParticleSig;
	std::vector<GraphicsPSO> ParticleSystem::mParticlePSOs;
	StructuredBuffer ParticleSystem::sParticleBuffer;
	DescriptorHandle ParticleSystem::sParticleGpuHandle;

	ComputePSO ParticleSystem::sParticleEmitCS;
	ComputePSO ParticleSystem::sParticleUpdateCS;
	ComputePSO ParticleSystem::sParticleDispatchIndirectArgsCS;
	ComputePSO ParticleSystem::sParticleFinalDispatchIndirectArgsCS;

	IndirectArgsBuffer ParticleSystem::sDrawIndirectArgs;
	IndirectArgsBuffer ParticleSystem::sDispatchIndirectArgs;
	IndirectArgsBuffer ParticleSystem::sFinalDispatchIndirectArgs;

	std::vector<std::unique_ptr<Particle>> ParticleSystem::sParticlePool;
	std::vector<Particle*> ParticleSystem::sParticlesActive;
	BlendMode ParticleSystem::sBlendMode = BlendMode::kBlendAlphaAdd;
	PrewView ParticleSystem::sPrewView;

	std::vector<std::pair<DescriptorHandle, TextureRef>> ParticleSystem::sTextures;
	std::map<std::wstring, uint32_t> ParticleSystem::sTextureArrayLookup;
	DescriptorHeap ParticleSystem::sTextureArrayHeap;

	bool ParticleSystem::sInitComplete = false;

	std::vector<ParticleProperty> ParticleSystem::sParticlePrefabs;
	struct InputVertex
	{
		Vector3 position;
		Vector2 uv;
	};
	void ParticleSystem::Initialize()
	{
		D3D12_SAMPLER_DESC SamplerBilinearBorderDesc = SamplerPointBorderDesc;
		SamplerBilinearBorderDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;

		mParticleSig.Reset(5, 1);
		mParticleSig.InitStaticSampler(0, SamplerPointBorderDesc);
		mParticleSig[0].InitAsConstantBuffer(0);
		mParticleSig[1].InitAsConstantBuffer(1);
		mParticleSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 5);
		mParticleSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
		mParticleSig[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 32, D3D12_SHADER_VISIBILITY_PIXEL, 1);

		mParticleSig.Finalize(L"Particle RootSig", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		D3D12_INPUT_ELEMENT_DESC inputLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		DXGI_FORMAT ColorFormat = gSceneColorBuffer.GetFormat();
		DXGI_FORMAT DepthFormat = gSceneDepthBuffer.GetFormat();

		auto vsBlob = ShaderCompiler::CompileBlob(L"/Particle/ParticleVS.hlsl", L"vs_6_2");
		auto psBlob = ShaderCompiler::CompileBlob(L"/Particle/ParticlePS.hlsl", L"ps_6_2");

		auto particleEmitBlob = ShaderCompiler::CompileBlob(L"/Particle/ParticleEmitCS.hlsl", L"cs_6_2");
		auto particleUpdateBlob = ShaderCompiler::CompileBlob(L"/Particle/ParticleUpdateCS.hlsl", L"cs_6_2");
		auto particleDispatchIndirectArgs = ShaderCompiler::CompileBlob(L"/Particle/ParticleDispatchIndirectArgs.hlsl", L"cs_6_2");
		auto particleFinalDispatchIndirectArgs = ShaderCompiler::CompileBlob(L"/Particle/ParticleFinalDispatchIndirectArgsCS.hlsli", L"cs_6_2");
		// PSO
		mParticlePSOs.resize(kNumBlendModes);
		for (uint32_t i = 0; i < kNumBlendModes; i++)
		{
			auto& pso = mParticlePSOs[i];

			pso.SetRootSignature(mParticleSig);
			pso.SetRasterizerState(RasterizerTwoSided);
			pso.SetBlendState(gBlendModeTable[BlendMode(i)]);
			pso.SetDepthStencilState(DepthStateReadOnly);
			pso.SetInputLayout(_countof(inputLayout), inputLayout);
			pso.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			pso.SetRenderTargetFormats(1, &ColorFormat, DepthFormat);
			pso.SetVertexShader(vsBlob.Get());
			pso.SetPixelShader(psBlob.Get());
			pso.Finalize();
		}
		//compute
		sParticleEmitCS.SetRootSignature(mParticleSig);
		sParticleEmitCS.SetComputeShader(particleEmitBlob.Get());
		sParticleEmitCS.Finalize();

		sParticleUpdateCS.SetRootSignature(mParticleSig);
		sParticleUpdateCS.SetComputeShader(particleUpdateBlob.Get());
		sParticleUpdateCS.Finalize();

		sParticleDispatchIndirectArgsCS.SetRootSignature(mParticleSig);
		sParticleDispatchIndirectArgsCS.SetComputeShader(particleDispatchIndirectArgs.Get());
		sParticleDispatchIndirectArgsCS.Finalize();

		sParticleFinalDispatchIndirectArgsCS.SetRootSignature(mParticleSig);
		sParticleFinalDispatchIndirectArgsCS.SetComputeShader(particleFinalDispatchIndirectArgs.Get());
		sParticleFinalDispatchIndirectArgsCS.Finalize();

		sParticleBuffer.Create(L"Particles Buffer", kMaxParticles, sizeof(ParticleVertex));

		__declspec(align(16)) UINT InitialDrawIndirectArgs[4] = { 6, 0, 0, 0 };
		sDrawIndirectArgs.Create(L"ParticleEffectManager DrawIndirectArgs", 1, sizeof(D3D12_DRAW_ARGUMENTS), InitialDrawIndirectArgs);
		__declspec(align(16)) UINT InitialDispatchIndirectArgs[6] = { 0, 1, 1, 0, 1, 1 };
		sFinalDispatchIndirectArgs.Create(L"ParticleEffectManager FinalDispatchIndirectArgs", 1, sizeof(D3D12_DISPATCH_ARGUMENTS), InitialDispatchIndirectArgs);

		sTextureArrayHeap.Create(L"ParticleSystem TextureArrayHeap", D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32);

		sParticleGpuHandle = sTextureArrayHeap.Alloc();
		uint32_t count = 1;
		uint32_t source[] = { 1 };
		D3D12_CPU_DESCRIPTOR_HANDLE sourceTex[] = { sParticleBuffer.GetSRV() };
		DX12Core::gDevice->CopyDescriptors(1, &sParticleGpuHandle,
			&count, count, sourceTex, source, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		sInitComplete = true;
	}

	void ParticleSystem::Update(ComputeContext& Context, float deltaTime)
	{
		if (!sInitComplete || sParticlesActive.size() == 0)return;

		sPrewView.deltaTime = deltaTime;
		sPrewView.time += deltaTime;

		Context.ResetCounter(sParticleBuffer);
		if (sParticlesActive.size() == 0)return;
		Context.SetRootSignature(mParticleSig);
		Context.TransitionResource(sParticleBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.SetDynamicConstantBufferView(0, sizeof(sPrewView), &sPrewView);
		Context.SetDynamicDescriptor(2, 0, sParticleBuffer.GetUAV());

		for (uint32_t i = 0; i < sParticlesActive.size(); ++i)
		{
			sParticlesActive[i]->Update(Context, deltaTime);

			if (sParticlesActive[i]->GetLifetime() <= sParticlesActive[i]->GetElapsedTime())
			{
				//Erase from vector
				auto iter = sParticlesActive.begin() + i;
				static std::mutex s_EraseEffectMutex;
				s_EraseEffectMutex.lock();
				sParticlesActive.erase(iter);
				s_EraseEffectMutex.unlock();
			}
		}

		SetFinalBuffers(Context);
	}

	void ParticleSystem::Render(GraphicsContext& gfxContext, const Camera& camera, ColorBuffer& ColorTarget, DepthBuffer& DepthTarget)
	{
		if (!sInitComplete || sParticlesActive.size() == 0)return;

		sPrewView.ViewProj = camera.GetViewProjMatrix();
		sPrewView.billboardMat = Matrix4x4(
			camera.GetRightVec(),
			camera.GetUpVec(),
			camera.GetForwardVec());

		D3D12_RECT scissor;
		scissor.left = 0;
		scissor.top = 0;
		scissor.right = (LONG)gSceneColorBuffer.GetWidth();
		scissor.bottom = (LONG)gSceneColorBuffer.GetHeight();

		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0.0;
		viewport.TopLeftY = 0.0;
		viewport.Width = (float)gSceneColorBuffer.GetWidth();
		viewport.Height = (float)gSceneColorBuffer.GetHeight();
		viewport.MinDepth = 0.0;
		viewport.MaxDepth = 1.0;


		gfxContext.SetRootSignature(mParticleSig);
		gfxContext.SetPipelineState(mParticlePSOs[sBlendMode]);
		gfxContext.TransitionResource(sParticleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		gfxContext.TransitionResource(sDrawIndirectArgs, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

		static const InputVertex verteices[6] = {
			{{-0.5f,-0.5f,0.0f},{0.0f,1.0f}},
			{{-0.5f,0.5f, 0.0f},{ 0.0f,0.0f}},
			{{0.5f,-0.5f, 0.0f},{ 1.0f,1.0f}},
			{{0.5f,-0.5f, 0.0f},{ 1.0f,1.0f}},
			{{-0.5f,0.5f, 0.0f},{ 0.0f,0.0f}},
			{{0.5f,0.5f,0.0f},{1.0f,0.0f}}
		};
		gfxContext.SetDynamicVB(0, _countof(verteices), sizeof(InputVertex), verteices);
		ID3D12DescriptorHeap* heaps[] = { sTextureArrayHeap.GetHeapPointer() };
		gfxContext.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, *heaps);

		gfxContext.SetDynamicConstantBufferView(0, sizeof(sPrewView), &sPrewView);
		gfxContext.SetDescriptorTable(3, sParticleGpuHandle);
		gfxContext.SetDescriptorTable(4, sTextureArrayHeap.GetHeapPointer()->GetGPUDescriptorHandleForHeapStart());

		gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		gfxContext.TransitionResource(ColorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		gfxContext.TransitionResource(DepthTarget, D3D12_RESOURCE_STATE_DEPTH_READ);
		gfxContext.SetRenderTarget(ColorTarget.GetRTV(), DepthTarget.GetDSV_DepthReadOnly());
		gfxContext.SetViewportAndScissor(viewport, scissor);

		gfxContext.DrawIndirect(sDrawIndirectArgs);
	}

	void ParticleSystem::Shutdown()
	{
		sParticlePool.clear();
		sParticlesActive.clear();
		sTextures.clear();

		sTextureArrayHeap.Destroy();
		sParticleBuffer.Destroy();
		sDrawIndirectArgs.Destroy();
		sDispatchIndirectArgs.Destroy();
		sFinalDispatchIndirectArgs.Destroy();
	}

	uint32_t ParticleSystem::CreateParticle(ParticleProperty& props)
	{
		if (!sInitComplete)
			return 0xffffffff;

		props.EmitProperties.TextureID = GetTextureIndex(props.TexturePath);


		static std::mutex s_InstantiateNewEffectMutex;
		s_InstantiateNewEffectMutex.lock();
		Particle* newEffect = new Particle(props);
		sParticlePool.emplace_back(newEffect);
		sParticlesActive.push_back(newEffect);
		s_InstantiateNewEffectMutex.unlock();

		uint32_t index = (uint32_t)sParticlesActive.size() - 1;
		sParticlesActive[index]->Initialize();
		return index;
	}

	uint32_t ParticleSystem::CreateParticlePrefab(const ParticleProperty& props)
	{
		if (!sInitComplete) return 0xffffffff;

		ParticleProperty p = props;
		// テクスチャは先に登録しておく
		p.EmitProperties.TextureID = GetTextureIndex(p.TexturePath);

		sParticlePrefabs.push_back(std::move(p));
		return (uint32_t)sParticlePrefabs.size() - 1;
	}

	uint32_t ParticleSystem::EmitPrefab(uint32_t prefabId, const Vector3& emitPosW)
	{
		if (!sInitComplete || prefabId >= sParticlePrefabs.size()) return 0xffffffff;

		ParticleProperty props = sParticlePrefabs[prefabId];
		props.EmitProperties.EmitPosW = emitPosW;
		props.EmitProperties.LastEmitPosW = emitPosW;

		static std::mutex s_emit_mutex;
		s_emit_mutex.lock();
		Particle* newEffect = new Particle(props);
		sParticlePool.emplace_back(newEffect);
		sParticlesActive.push_back(newEffect);
		s_emit_mutex.unlock();

		uint32_t index = (uint32_t)sParticlesActive.size() - 1;
		sParticlesActive[index]->Initialize();
		return index;
	}
	void ParticleSystem::ResetParticle(uint32_t particleId)
	{
		if (!sInitComplete || sParticlesActive.size() == 0 || particleId >= sParticlesActive.size())
			return;

		sParticlesActive[particleId]->Reset();
	}

	float ParticleSystem::GetCurrentLife(uint32_t particleId)
	{
		if (!sInitComplete || sParticlesActive.size() == 0 || particleId >= sParticlesActive.size())
			return -1.0;

		return sParticlesActive[particleId]->GetElapsedTime();
	}

	void ParticleSystem::EmitParticle(uint32_t particleId, const Vector3& emitPosW)
	{
		if (!sInitComplete || sParticlesActive.size() == 0 || particleId >= sParticlesActive.size())
			return;

		
	}

	void ParticleSystem::SetFinalBuffers(ComputeContext& CompContext)
	{
		CompContext.SetPipelineState(sParticleFinalDispatchIndirectArgsCS);
		CompContext.TransitionResource(sParticleBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
		CompContext.TransitionResource(sFinalDispatchIndirectArgs, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CompContext.TransitionResource(sDrawIndirectArgs, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CompContext.SetDynamicDescriptor(2, 0, sFinalDispatchIndirectArgs.GetUAV());
		CompContext.SetDynamicDescriptor(2, 1, sDrawIndirectArgs.GetUAV());
		CompContext.SetDynamicDescriptor(3, 0, sParticleBuffer.GetCounterSRV(CompContext));

		CompContext.Dispatch(1, 1, 1);
	}

	uint32_t ParticleSystem::GetTextureIndex(const std::wstring& name)
	{
		if (sTextureArrayLookup.contains(name))
		{
			return sTextureArrayLookup[name];
		}

		auto handle = sTextureArrayHeap.Alloc();

		TextureRef texture = AssetManager::LoadCovertTexture(name, kMagenta2D, true);
		texture.CopyGPU(sTextureArrayHeap.GetHeapPointer(), handle);
		sTextures.push_back(std::make_pair(handle, texture));
		uint32_t index = sTextureArrayHeap.GetOffsetOfHandle(handle);
		sTextureArrayLookup[name] = index;

		return index;
	}

	static Vector4 json_to_vec4_local(const json& j)
	{
		Vector4 v;
		if (j.is_array() && j.size() >= 4) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); v.w = j[3].get<float>(); }
		return v;
	}
	static Vector3 json_to_vec3_local(const json& j)
	{
		Vector3 v;
		if (j.is_array() && j.size() >= 3) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); }
		return v;
	}
	static Vector2 json_to_vec2_local(const json& j)
	{
		Vector2 v;
		if (j.is_array() && j.size() >= 2) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); }
		return v;
	}

	bool ParticleSystem::ParseParticlePropertyFromJson(const std::string& text, ParticleProperty& out)
	{
		try
		{
			auto j = json::parse(text);

			if (j.contains("MinStartColor")) out.MinStartColor = json_to_vec4_local(j["MinStartColor"]);
			if (j.contains("MaxStartColor")) out.MaxStartColor = json_to_vec4_local(j["MaxStartColor"]);
			if (j.contains("MinEndColor")) out.MinEndColor = json_to_vec4_local(j["MinEndColor"]);
			if (j.contains("MaxEndColor")) out.MaxEndColor = json_to_vec4_local(j["MaxEndColor"]);

			if (j.contains("Velocity")) out.Velocity = json_to_vec4_local(j["Velocity"]);
			if (j.contains("Size")) out.Size = json_to_vec4_local(j["Size"]);
			if (j.contains("Spread")) out.Spread = json_to_vec3_local(j["Spread"]);
			if (j.contains("EmitRate")) out.EmitRate = j["EmitRate"].get<float>();
			if (j.contains("LifeMinMax")) out.LifeMinMax = json_to_vec2_local(j["LifeMinMax"]);
			if (j.contains("MassMinMax")) out.MassMinMax = json_to_vec2_local(j["MassMinMax"]);
			if (j.contains("TotalActiveLifetime")) out.TotalActiveLifetime = j["TotalActiveLifetime"].get<float>();

			if (j.contains("EmitProperties"))
			{
				auto e = j["EmitProperties"];
				if (e.contains("LastEmitPosW")) out.EmitProperties.LastEmitPosW = json_to_vec3_local(e["LastEmitPosW"]);
				if (e.contains("EmitSpeed")) out.EmitProperties.EmitSpeed = e["EmitSpeed"].get<float>();
				if (e.contains("EmitPosW")) out.EmitProperties.EmitPosW = json_to_vec3_local(e["EmitPosW"]);
				if (e.contains("FloorHeight")) out.EmitProperties.FloorHeight = e["FloorHeight"].get<float>();
				if (e.contains("EmitDirW")) out.EmitProperties.EmitDirW = json_to_vec3_local(e["EmitDirW"]);
				if (e.contains("Restitution")) out.EmitProperties.Restitution = e["Restitution"].get<float>();
				if (e.contains("EmitRightW")) out.EmitProperties.EmitRightW = json_to_vec3_local(e["EmitRightW"]);
				if (e.contains("EmitterVelocitySensitivity")) out.EmitProperties.EmitterVelocitySensitivity = e["EmitterVelocitySensitivity"].get<float>();
				if (e.contains("EmitUpW")) out.EmitProperties.EmitUpW = json_to_vec3_local(e["EmitUpW"]);
				if (e.contains("MaxParticles")) out.EmitProperties.MaxParticles = e["MaxParticles"].get<uint32_t>();
				if (e.contains("Gravity")) out.EmitProperties.Gravity = json_to_vec3_local(e["Gravity"]);
				if (e.contains("EmissiveColor")) out.EmitProperties.EmissiveColor = json_to_vec3_local(e["EmissiveColor"]);
			}

			if (j.contains("TexturePath"))
				out.TexturePath = UTF8ToWString(j["TexturePath"].get<std::string>());

			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	uint32_t ParticleSystem::CreateParticleFromFile(const std::string& utf8Path)
	{
		try
		{
			std::ifstream ifs(utf8Path, std::ios::binary);
			if (!ifs) return 0xffffffff;
			std::ostringstream ss;
			ss << ifs.rdbuf();
			auto content = ss.str();
			ifs.close();

			ParticleProperty props;
			if (!ParseParticlePropertyFromJson(content, props)) return 0xffffffff;

			return CreateParticle(props);
		}
		catch (...)
		{
			return 0xffffffff;
		}
	}
}
