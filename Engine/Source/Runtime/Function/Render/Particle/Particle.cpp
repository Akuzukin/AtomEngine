#include "Particle.h"
#include "ParticleSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Math/Random.h"

namespace AtomEngine
{
	Particle::Particle(ParticleProperty& Property)
	{
		mProperty = Property;
		mElapsedTime = 0.0f;
		if (mProperty.TotalActiveLifetime == 0)
			mProperty.TotalActiveLifetime = FLT_MAX;
		mCurrentStateBuffer = 0;
	}

	Particle::~Particle()
	{
		mStateBuffers[0].Destroy();
		mStateBuffers[1].Destroy();
		mInitBuffer.Destroy();
		mDispatchIndirectArgs.Destroy();
	}

	// ランタイムでプロパティを更新
	ParticleProperty& Particle::GetProperty()
	{
		return mProperty;
	}

	const ParticleProperty& Particle::GetProperty() const
	{
		return mProperty;
	}

	void Particle::SetProperty(const ParticleProperty& prop)
	{
		// 単純に内部プロパティを置き換える
		mProperty = prop;
		mOriginalProperty = prop;
	}

	void Particle::Emit(const Vector3& emitPosW)
	{
		mProperty.EmitProperties.Emit = 1;
		mProperty.EmitProperties.EmitPosW = emitPosW;
		mProperty.EmitProperties.LastEmitPosW = emitPosW;
	}

	inline static Vector3 RandSpread(const Vector3& s)
	{
		return Vector3(
			Random::uniform(-s.x, s.x),
			Random::uniform(-s.y, s.y),
			Random::uniform(-s.z, s.z)
		);
	}

	inline static Vector4 RandColor(Vector4 c0, Vector4 c1)
	{
		return Vector4(
			Random::uniform(c0.x, c1.x),
			Random::uniform(c0.y, c1.y),
			Random::uniform(c0.z, c1.z),
			Random::uniform(c0.w, c1.w)
		);
	}

	void Particle::Initialize()
	{
		mOriginalProperty = mProperty;

		ParticleEmitData* pSpawnData = new ParticleEmitData[mProperty.EmitProperties.MaxParticles];
		for (UINT i = 0; i < mProperty.EmitProperties.MaxParticles; i++)
		{
			ParticleEmitData rd = {};

			rd.AgeRate = 1.0f / Random::uniform(mProperty.LifeMinMax.x, mProperty.LifeMinMax.y);
			rd.SpreadOffset = RandSpread(mProperty.Spread);

			switch (mProperty.EmitterType)
			{
			case 0:
			{
				float horizontalAngle = Random::uniform(0.0f, Math::TwoPI);
				float horizontalVelocity = Random::uniform(mProperty.Velocity.x, mProperty.Velocity.y);

				rd.Velocity.x = horizontalVelocity * cos(horizontalAngle);
				rd.Velocity.y = Random::uniform(mProperty.Velocity.z, mProperty.Velocity.w);
				rd.Velocity.z = horizontalVelocity * sin(horizontalAngle);
				break;
			}
			case 1:
			{
				float theta = Random::uniform(0.0f, Math::TwoPI);
				float u = Random::uniform(-1.0f, 1.0f);
				float r = Random::uniform(mProperty.Velocity.x, mProperty.Velocity.y);

				float sqrtTerm = sqrt(1.0f - u * u);
				rd.Velocity.x = r * sqrtTerm * cos(theta);
				rd.Velocity.y = r * u;
				rd.Velocity.z = r * sqrtTerm * sin(theta);
				break;
			}
			case 2:
			default:
			{
				auto SampleSphere = [](const Vector3& extents) -> Vector3
					{
						Vector3 dir;
						do
						{
							dir = Vector3(
								Random::uniform(-1.0f, 1.0f),
								Random::uniform(-1.0f, 1.0f),
								Random::uniform(-1.0f, 1.0f));
						} while (dir.IsZeroLength() || dir.LengthSqr() > 1.0f);

						dir.Normalize();
						float radius = std::cbrt(Random::uniform(0.0f, 1.0f));
						Vector3 sample = dir * radius;
						return Vector3(sample.x * extents.x, sample.y * extents.y, sample.z * extents.z);
					};

				rd.SpreadOffset = SampleSphere(mProperty.Spread);

				Vector3 dirToCenter = -rd.SpreadOffset;
				if (dirToCenter.IsZeroLength())
				{
					dirToCenter = Vector3::UP;
				}
				dirToCenter.Normalize();

				const float radialSpeed = Random::uniform(mProperty.Velocity.x, mProperty.Velocity.y);
				const float verticalSpeed = Random::uniform(mProperty.Velocity.z, mProperty.Velocity.w);

				rd.Velocity = dirToCenter * radialSpeed;
				rd.Velocity.y += verticalSpeed;

				Vector3 swirlAxis = dirToCenter.Cross(Vector3::UP);
				if (!swirlAxis.IsZeroLength())
				{
					swirlAxis.Normalize();
					const float swirlStrength = Random::uniform(-1.0f, 1.0f) * mProperty.EmitProperties.EmitSpeed;
					rd.Velocity += swirlAxis * swirlStrength;
				}
				break;
			}
			}


			rd.StartSize = Random::uniform(mProperty.Size.x, mProperty.Size.y);
			rd.EndSize = Random::uniform(mProperty.Size.z, mProperty.Size.w);
			rd.StartColor = RandColor(mProperty.MinStartColor, mProperty.MaxStartColor);
			rd.EndColor = RandColor(mProperty.MinEndColor, mProperty.MaxEndColor);
			rd.Mass = Random::uniform(mProperty.MassMinMax.x, mProperty.MassMinMax.y);
			rd.RotationSpeed = Random::uniform_unit();
			rd.Random = Random::uniform_unit();
			rd.Drag = mProperty.drag;
			rd.DragFactor = mProperty.dragFactor;

			pSpawnData[i] = rd;
		}
		mInitBuffer.Create(L"Particle Init Buffer", mProperty.EmitProperties.MaxParticles, sizeof(ParticleEmitData), pSpawnData);
		delete[] pSpawnData;

		mStateBuffers[0].Create(L"Particle Buffer0", mProperty.EmitProperties.MaxParticles, sizeof(ParticleMotion));
		mStateBuffers[1].Create(L"Particle Buffer1", mProperty.EmitProperties.MaxParticles, sizeof(ParticleMotion));
		mCurrentStateBuffer = 0;

		__declspec(align(16)) UINT DispatchIndirectData[3] = { 0, 1, 1 };
		mDispatchIndirectArgs.Create(L"Particle DispatchIndirectArgs", 1, sizeof(D3D12_DISPATCH_ARGUMENTS), DispatchIndirectData);

	}

	void Particle::Update(ComputeContext& CompContext, float deltaTime)
	{
		if (deltaTime == 0.0f) return;

		mElapsedTime += deltaTime;
		mProperty.EmitProperties.LastEmitPosW = mProperty.EmitProperties.EmitPosW;

		for (uint32_t i = 0; i < 64; i++)
		{
			UINT random = (UINT)Random::uniform_int(mProperty.EmitProperties.MaxParticles - 1);
			mProperty.EmitProperties.RandIndex[i].x = random;
		}

		CompContext.SetDynamicConstantBufferView(1, sizeof(EmitterProperty), &mProperty.EmitProperties);
		CompContext.TransitionResource(mStateBuffers[mCurrentStateBuffer], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		CompContext.SetDynamicDescriptor(3, 0, mInitBuffer.GetSRV());
		CompContext.SetDynamicDescriptor(3, 1, mStateBuffers[mCurrentStateBuffer].GetSRV());

		mCurrentStateBuffer ^= 1;

		CompContext.ResetCounter(mStateBuffers[mCurrentStateBuffer]);

		//Update パス
		CompContext.SetPipelineState(ParticleSystem::sParticleUpdateCS);
		CompContext.TransitionResource(mStateBuffers[mCurrentStateBuffer], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CompContext.TransitionResource(mDispatchIndirectArgs, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		CompContext.SetDynamicDescriptor(2, 2, mStateBuffers[mCurrentStateBuffer].GetUAV());
		CompContext.DispatchIndirect(mDispatchIndirectArgs, 0);

		CompContext.InsertUAVBarrier(mStateBuffers[mCurrentStateBuffer]);

		//Emit パス
		if (mProperty.EmitProperties.Emit)
		{
			CompContext.SetPipelineState(ParticleSystem::sParticleEmitCS);
			CompContext.SetDynamicDescriptor(3, 0, mInitBuffer.GetSRV());
			UINT NumToSpawn = (UINT)(mProperty.EmitRate * deltaTime);
			UINT NumSpawnThreads = (NumToSpawn + 63) / 64;
			CompContext.Dispatch(NumSpawnThreads, 1, 1);
		}



		//Dispatch indirect args 
		CompContext.SetPipelineState(ParticleSystem::sParticleDispatchIndirectArgsCS);
		CompContext.TransitionResource(mStateBuffers[mCurrentStateBuffer], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		CompContext.TransitionResource(mDispatchIndirectArgs, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CompContext.SetDynamicDescriptor(3, 0, mStateBuffers[mCurrentStateBuffer].GetCounterSRV(CompContext));
		CompContext.SetDynamicDescriptor(2, 1, mDispatchIndirectArgs.GetUAV());
		CompContext.Dispatch(1, 1, 1);
	}

	void Particle::Reset()
	{
		mProperty = mOriginalProperty;
	}
}
