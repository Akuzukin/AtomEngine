#pragma once
#include "ParticleSystem.h"
#include "ParticleSettingsCodec.h"
#include "Runtime/Function/Framework/ECS/World.h"
#include "Runtime/Resource/AssetArchive.h"
#include "Runtime/Core/Utility/Utility.h"

namespace AtomEngine
{

	inline void EmitSceneParticle(World& world, Entity entity, const Vector3& position)
	{
		auto component = world.TryGet<ParticleComponent>(entity);
		if (!component) throw std::runtime_error("Object has no particle component");
		if (ParticleSystem::GetCurrentLife(component->runtimeId) < 0)
		{
			auto settings = component->preset.empty() ? component->settings : ReadParticleSettings(ReadAssetText(UTF8ToWString(component->preset)));
			component->runtimeId = ParticleSystem::Create(settings);
		}
		ParticleSystem::EmitParticle(component->runtimeId, position);
	}
}
