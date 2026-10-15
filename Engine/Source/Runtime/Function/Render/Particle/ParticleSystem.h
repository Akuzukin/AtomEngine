#pragma once
#include <array>
#include <vector>
#include "ParticleSettings.h"
#include "Runtime/Function/Camera/CameraBase.h"
#include "Runtime/Platform/DirectX12/Context/ComputeContext.h"
#include "Runtime/Platform/DirectX12/Context/GraphicsContext.h"
namespace AtomEngine
{
	// Generational emitter handles; each emitter owns fixed-capacity GPU storage.
	class ParticleSystem
	{
	public:
		static void Initialize(); static void Shutdown();
		static uint32_t Create(const ParticleSettings& settings, bool preview = false);
		static void EmitParticle(uint32_t handle, const Vector3& position);
		static void StopParticle(uint32_t handle); static void ReleaseParticle(uint32_t handle);
		static float GetCurrentLife(uint32_t handle);
		static uint32_t ReadAliveCountForDiagnostics(uint32_t handle);
        static std::vector<std::array<float,12>> ReadSpawnSamplesForDiagnostics(uint32_t handle);
		static void Update(ComputeContext& context, float dt);
		static void UpdatePreview(ComputeContext& context, uint32_t handle, float dt);
		static void Render(GraphicsContext& context, const Camera& camera, ColorBuffer& color, DepthBuffer& depth);
		static void RenderPreview(GraphicsContext& context, uint32_t handle, const Camera& camera, ColorBuffer& color, DepthBuffer& depth);
	};
}
