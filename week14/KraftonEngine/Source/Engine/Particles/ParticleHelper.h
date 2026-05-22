#pragma once

// ParticleData : uint8* 타입 포인터라고 가정
// ParticleStride : particle 하나의 byte 크기
// ActiveParticles : 현재 살아있는 particle 수


#define DECLARE_PARTICLE_PTR(ParticleName, ParticleData) \
	FBaseParticle* ParticleName = reinterpret_cast<FBaseParticle*>(ParticleData)
#define BEGIN_UPDATE_LOOP \
	for(int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; ++ParticleIndex) \
	{\
		FBaseParticle& Particle = *ParticlePtr;
	
#define END_UPDATE_LOOP \
		ParticleData += ParticleStride; \
	}