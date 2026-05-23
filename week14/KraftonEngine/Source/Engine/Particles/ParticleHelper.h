#pragma once

// ParticleData : uint8* 타입 포인터라고 가정
// ParticleStride : particle 하나의 byte 크기
// ActiveParticles : 현재 살아있는 particle 수

#define DECLARE_PARTICLE_PTR(Name, Address) \
	FBaseParticle* Name = (FBaseParticle*)(Address);

#define BEGIN_UPDATE_LOOP \
	{ \
		int32& ActiveParticles = Context.Owner.ActiveParticles; \
		int32 Offset = Context.Offset; \
		uint32 CurrentOffset = Offset; \
		float DeltaTime = Context.DeltaTime; \
		uint8* ParticleData = Context.Owner.ParticleData; \
		const uint32 ParticleStride = Context.Owner.ParticleStride; \
		uint16* ParticleIndices = Context.Owner.ParticleIndices; \
		for (int32 ParticleIndex = ActiveParticles - 1; ParticleIndex >= 0; --ParticleIndex) \
		{ \
			const int32 CurrentIndex = ParticleIndices[ParticleIndex]; \
			uint8* ParticleBase = ParticleData + CurrentIndex * ParticleStride; \
			DECLARE_PARTICLE_PTR(Particle, ParticleBase)
	
#define END_UPDATE_LOOP \
			CurrentOffset = Offset; \
		} \
	}
