#pragma once

#include "Core/CoreMinimal.h"
#include "Particle/ParticleTypes.h"

#include <cstdint>

namespace ParticleHelper
{
	// 16바이트도 되고 4바이트로 맞출 수도 있기는 한데, SIMD 최적화를 고려해서 16바이트로 맞춤
	static constexpr int32 ParticleAlignment = 16;

	/**
     * @brief 주어진 값이 ParticleAlignment의 배수가 되도록 올림하여 반환
	 */
	inline int32 AlignParticleSize(int32 Value, int32 Alignment = ParticleAlignment)
	{
		if (Alignment <= 0)
		{
			return Value;
		}

		return ((Value + Alignment - 1) / Alignment) * Alignment;
	}

	/**
     * @brief 주어진 포인터가 ParticleAlignment의 배수 주소가 되도록 올림하여 반환
	 */
	inline uint8* AlignParticlePointer(uint8* Ptr, int32 Alignment = ParticleAlignment)
	{
		if (Ptr == nullptr || Alignment <= 0)
		{
			return Ptr;
		}

		const std::uintptr_t Address = reinterpret_cast<std::uintptr_t>(Ptr);
		const std::uintptr_t AlignedAddress = (Address + Alignment - 1) & ~(static_cast<std::uintptr_t>(Alignment) - 1);
		return reinterpret_cast<uint8*>(AlignedAddress);
	}
}

/**
 * @brief particle 업데이트 루프와 spawn 루프를 간편하게 작성하기 위한 매크로들
 */

#define DECLARE_PARTICLE_PTR(Owner, ActiveIndex, ParticleName) \
	FBaseParticle& ParticleName = (Owner)->GetParticleByActiveIndex(ActiveIndex)

#define BEGIN_UPDATE_LOOP(Owner, ParticleName) \
	for (int32 ParticleActiveIndex = 0; ParticleActiveIndex < (Owner)->GetActiveParticleCount(); ++ParticleActiveIndex) \
	{ \
		DECLARE_PARTICLE_PTR(Owner, ParticleActiveIndex, ParticleName);

#define END_UPDATE_LOOP() \
	}

#define BEGIN_SPAWN_LOOP(Count, SpawnIndexName) \
	for (int32 SpawnIndexName = 0; SpawnIndexName < (Count); ++SpawnIndexName) \
	{

#define END_SPAWN_LOOP() \
	}
