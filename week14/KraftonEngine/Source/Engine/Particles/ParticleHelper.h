#pragma once

// ParticleData : uint8* 타입 포인터라고 가정
// ParticleStride : particle 하나의 byte 크기
// ActiveParticles : 현재 살아있는 particle 수

#define DECLARE_PARTICLE_PTR(Type, Name) \
	Type* Name = (Type*)(ParticleData + Offset);

#define BEGIN_UPDATE_LOOP \
	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; ++ParticleIndex) \
	{ \
		FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(ParticleData);
	
#define END_UPDATE_LOOP \
		ParticleData += ParticleStride; \
	}
