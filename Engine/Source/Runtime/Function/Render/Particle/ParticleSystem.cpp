#include "Runtime/Platform/DirectX12/Buffer/ReadbackBuffer.h"
#include "ParticleSettingsCodec.h"
#include "ParticleSystem.h"
#include "Runtime/Platform/DirectX12/Core/DirectX12Core.h"
#include "Runtime/Platform/DirectX12/Shader/ShaderCompiler.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Platform/DirectX12/Buffer/BufferManager.h"
#include <vector>
#include <cmath>
#include <array>
#include <algorithm>
namespace AtomEngine
{
	namespace
	{
		struct GpuParticle { Vector4 positionAge, velocityLife, colorSize; };
		struct alignas(16) Parameters
		{
			Matrix4x4 viewProj; Vector4 right, up, originDt, gravityLife, startColor, endColor, extentRadius, controls;
			uint32_t capacity, spawnStart, spawnCount, shape; float speed, drag, cone, time; uint32_t billboard, textured, pad0, pad1;
		};
		struct Instance { ParticleSettings settings; StructuredBuffer buffer; TextureRef texture; Vector3 position{}; float elapsed = 0, remainder = 0; uint32_t cursor = 0, pending = 0; bool emitting = false, preview = false; };
		static_assert(sizeof(GpuParticle)==48 && sizeof(Parameters)==240, "Particle HLSL layout mismatch");
		struct Slot { std::unique_ptr<Instance> value; uint32_t generation = 1; };
		struct Retired { std::unique_ptr<Instance> value; std::array<uint64_t,3> fences; };
		std::vector<Retired> retired;
		void CollectRetired()
		{
			std::erase_if(retired, [](const Retired &item) {
				return std::all_of(item.fences.begin(),item.fences.end(), [](uint64_t fence) {
					return DX12Core::gCommandManager.IsFenceComplete(fence);
				});
			});
		}
		std::vector<Slot> slots; RootSignature root; ComputePSO simulation; GraphicsPSO renderers[4][2]; bool initialized = false;
		Instance* Get(uint32_t id) { uint32_t index = id & 65535, generation = id >> 16; return index < slots.size() && slots[index].generation == generation ? slots[index].value.get() : nullptr; }
		Parameters Constants(Instance& e) { Parameters p{}; p.originDt = { e.position.x,e.position.y,e.position.z,0 }; p.gravityLife = { e.settings.gravity.x,e.settings.gravity.y,e.settings.gravity.z,e.settings.lifetime }; p.startColor = e.settings.colorStart; p.endColor = e.settings.colorEnd; p.extentRadius = { e.settings.extent.x,e.settings.extent.y,e.settings.extent.z,e.settings.radius }; p.controls = { e.settings.sizeStart,e.settings.sizeEnd,float(e.settings.blend),0 }; p.capacity = e.settings.capacity; p.shape = uint32_t(e.settings.shape); p.speed = e.settings.speed; p.drag = e.settings.drag; p.cone = e.settings.coneAngle; p.time = e.elapsed; p.billboard = e.settings.billboard; p.textured = !e.settings.texture.empty(); return p; }
		void Advance(ComputeContext& ctx, Instance& e, float dt)
		{
			if (dt <= 0)return; dt = std::min(dt, .1f); auto p = Constants(e); p.originDt.w = dt;
			double generated = 0; if (e.emitting && (e.settings.loop || e.elapsed < e.settings.duration))generated = e.settings.rate * (e.settings.loop ? dt : std::min(dt, std::max(0.f, e.settings.duration - e.elapsed))) + e.remainder;
			uint32_t n = uint32_t(generated); e.remainder = float(generated - n); p.spawnCount = std::min(e.settings.capacity, n + e.pending); e.pending = 0; p.spawnStart = e.cursor; e.cursor = (e.cursor + p.spawnCount) % e.settings.capacity; e.elapsed += dt;
			ctx.SetRootSignature(root); ctx.SetPipelineState(simulation); ctx.TransitionResource(e.buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS); ctx.SetDynamicConstantBufferView(0, sizeof(p), &p); ctx.SetBufferUAV(1, e.buffer); ctx.Dispatch1D(e.settings.capacity, 64); ctx.InsertUAVBarrier(e.buffer);
		}
		void Draw(GraphicsContext& ctx, Instance& e, const Camera& camera, ColorBuffer& color, DepthBuffer& depth)
		{
			auto p = Constants(e); p.viewProj = camera.GetViewProjMatrix(); auto r = camera.GetRightVec(), u = camera.GetUpVec(); p.right = { r.x,r.y,r.z,0 }; p.up = { u.x,u.y,u.z,0 };
			ctx.SetRootSignature(root); ctx.SetPipelineState(renderers[uint32_t(e.settings.blend)][color.GetFormat() == DXGI_FORMAT_R8G8B8A8_UNORM ? 1 : 0]); ctx.TransitionResource(e.buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); ctx.TransitionResource(color, D3D12_RESOURCE_STATE_RENDER_TARGET); ctx.TransitionResource(depth, D3D12_RESOURCE_STATE_DEPTH_READ); ctx.SetRenderTarget(color.GetRTV(), depth.GetDSV_DepthReadOnly()); ctx.SetViewportAndScissor(0, 0, color.GetWidth(), color.GetHeight()); ctx.SetDynamicConstantBufferView(0, sizeof(p), &p); ctx.SetBufferSRV(2, e.buffer); ctx.SetDynamicDescriptor(3, 0, e.settings.texture.empty() ? GetDefaultTexture(kWhiteOpaque2D) : e.texture.GetSRV()); ctx.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); ctx.DrawInstanced(6, e.settings.capacity);
		}
	}
	void ParticleSystem::Initialize()
	{
		root.Reset(4, 1); root.InitStaticSampler(0, SamplerLinearClampDesc, D3D12_SHADER_VISIBILITY_PIXEL); root[0].InitAsConstantBuffer(0); root[1].InitAsBufferUAV(0); root[2].InitAsBufferSRV(0); root[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL); root.Finalize(L"GPU particles v2");
		simulation.SetRootSignature(root); simulation.SetComputeShader(ShaderCompiler::CompileBlob(L"ParticleV2CS.hlsl", L"cs_6_2").Get()); simulation.Finalize();
		auto vs = ShaderCompiler::CompileBlob(L"ParticleV2VS.hlsl", L"vs_6_2"), ps = ShaderCompiler::CompileBlob(L"ParticleV2PS.hlsl", L"ps_6_2");
		for (int blend = 0; blend < 4; blend++)for (int format = 0; format < 2; format++) { auto& p = renderers[blend][format]; p.SetRootSignature(root); p.SetRasterizerState(RasterizerTwoSided); auto b = BlendTraditional; if (blend == 1) { b.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA; b.RenderTarget[0].DestBlend = D3D12_BLEND_ONE; }if (blend == 2) { b.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR; b.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; }if (blend == 3)b = BlendDisable; p.SetBlendState(b); p.SetDepthStencilState(DepthStateReadOnly); p.SetInputLayout(0, nullptr); p.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE); p.SetRenderTargetFormat(format ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT); p.SetVertexShader(vs.Get()); p.SetPixelShader(ps.Get()); p.Finalize(); }initialized = true;
	}
	uint32_t ParticleSystem::Create(const ParticleSettings& s, bool preview) { if (!initialized || s.capacity == 0 || s.capacity > 65536 || s.lifetime <= 0 || s.duration <= 0 || uint32_t(s.shape) > 4 || uint32_t(s.blend) > 3)throw std::runtime_error("Invalid particle settings"); auto validated = ReadParticleSettings(ParticleJson(s).dump()); auto e = std::make_unique<Instance>(); e->settings = validated; e->preview = preview; if (!s.texture.empty())e->texture = AssetManager::LoadCovertTexture(s.texture, kWhiteOpaque2D, true); std::vector<GpuParticle> zero(s.capacity); for (auto& p : zero)p.positionAge.w = -1; e->buffer.Create(L"Particle component storage", s.capacity, sizeof(GpuParticle), zero.data()); size_t i = 0; while (i < slots.size() && slots[i].value)++i; if (i >= 65535)throw std::runtime_error("Particle emitter limit"); if (i == slots.size())slots.emplace_back(); slots[i].value = std::move(e); return uint32_t(i) | (slots[i].generation << 16); }
	void ParticleSystem::EmitParticle(uint32_t h, const Vector3& pos) { auto e = Get(h); if (!e)throw std::runtime_error("Stale particle handle"); e->position = pos; e->elapsed = 0; e->remainder = 0; e->pending = std::min(e->settings.burst, e->settings.capacity); e->emitting = true; }
	void ParticleSystem::StopParticle(uint32_t h) { if (auto e = Get(h)) { e->emitting = false; e->pending = 0; } }
	void ParticleSystem::ReleaseParticle(uint32_t h)
	{
		if (!Get(h)) return;
		auto &manager=DX12Core::gCommandManager;
		auto &slot=slots[h & 65535];
		// The last submitted commands keep their storage until all queues finish using it.
		retired.push_back({std::move(slot.value),{manager.GetGraphicsQueue().GetNextFenceValue()-1,
			manager.GetComputeQueue().GetNextFenceValue()-1,manager.GetCopyQueue().GetNextFenceValue()-1}});
		slot.generation=(slot.generation % 65534)+1;
	}
	float ParticleSystem::GetCurrentLife(uint32_t h) { auto e = Get(h); return e ? e->elapsed : -1; }
	void ParticleSystem::Update(ComputeContext& c, float dt) { CollectRetired();for (auto& s : slots)if (s.value && !s.value->preview)Advance(c, *s.value, dt); }
	void ParticleSystem::UpdatePreview(ComputeContext& c, uint32_t h, float dt) { CollectRetired();if (auto e = Get(h))Advance(c, *e, dt); }
	void ParticleSystem::Render(GraphicsContext& c, const Camera& cam, ColorBuffer& color, DepthBuffer& depth) { for (auto& s : slots)if (s.value && !s.value->preview)Draw(c, *s.value, cam, color, depth); }
	void ParticleSystem::RenderPreview(GraphicsContext& c, uint32_t h, const Camera& cam, ColorBuffer& color, DepthBuffer& depth) { if (auto e = Get(h))Draw(c, *e, cam, color, depth); }
	uint32_t ParticleSystem::ReadAliveCountForDiagnostics(uint32_t h) { auto e = Get(h); if (!e)return 0; ReadbackBuffer read; read.Create(L"Particle diagnostics", e->settings.capacity, sizeof(GpuParticle)); auto& ctx = GraphicsContext::Begin(L"Particle readback"); ctx.TransitionResource(e->buffer, D3D12_RESOURCE_STATE_COPY_SOURCE); ctx.CopyBufferRegion(read, 0, e->buffer, 0, e->settings.capacity * sizeof(GpuParticle)); ctx.Finish(true); auto p = static_cast<const GpuParticle*>(read.Map()); uint32_t count = 0; for (uint32_t i = 0; i < e->settings.capacity; i++)if (p[i].positionAge.w >= 0 && p[i].velocityLife.w > 0)count++; read.Unmap(); return count; }
	void ParticleSystem::Shutdown() { DX12Core::gCommandManager.IdleGPU();slots.clear();retired.clear(); initialized = false; }
}
