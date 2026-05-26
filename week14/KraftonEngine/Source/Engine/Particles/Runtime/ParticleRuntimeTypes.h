#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

struct FBaseParticle
{
	FVector Position;
	FVector OldPosition;
	FVector Velocity;

	FVector Size;
	float Rotation = 0.0f;
	float RotationRate = 0.0f;

	FVector4 Color = { 1, 1, 1, 1 };

	float RelativeTime = 0.0f;
	float OneOverMaxLifetime = 1.0f;
	float Lifetime = 1.0f;
	float Age = 0.0f;

	uint32 RandomSeed = 0;
	uint32 FrameIndex = 0;
	bool bAlive = false;
};

struct FParticleDataContainer
{
	FParticleDataContainer() = default;
	~FParticleDataContainer()
	{
		Release();
	}

	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

	void Initialize(int32 InMaxParticleCount, int32 InParticleStride)
	{
		Release();

		ParticleStride = InParticleStride;
		const int32 MaxParticleCount = InMaxParticleCount > 0 ? InMaxParticleCount : 0;
		ParticleDataNumBytes = MaxParticleCount * ParticleStride;
		ParticleIndicesNumShorts = MaxParticleCount;
		MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * static_cast<int32>(sizeof(uint16));

		if (MemBlockSize <= 0)
		{
			return;
		}

		ParticleData = new uint8[MemBlockSize]();
		ParticleIndices = reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);
	}

	void Release()
	{
		delete[] ParticleData;

		MemBlockSize = 0;
		ParticleDataNumBytes = 0;
		ParticleIndicesNumShorts = 0;
		ParticleData = nullptr;
		ParticleIndices = nullptr;
		ParticleStride = 0;
	}

	const FBaseParticle* GetParticles() const
	{
		return reinterpret_cast<const FBaseParticle*>(ParticleData);
	}

	FBaseParticle* GetMutableParticles()
	{
		return reinterpret_cast<FBaseParticle*>(ParticleData);
	}

	const uint8* GetParticleData() const { return ParticleData; }
	uint8* GetMutableParticleData() { return ParticleData; }

	const FBaseParticle& GetParticle(int32 ParticleIndex) const
	{
		return *reinterpret_cast<const FBaseParticle*>(ParticleData + ParticleIndex * ParticleStride);
	}

	FBaseParticle& GetMutableParticle(int32 ParticleIndex)
	{
		return *reinterpret_cast<FBaseParticle*>(ParticleData + ParticleIndex * ParticleStride);
	}

	const uint16* GetParticleIndices() const { return ParticleIndices; }
	uint16* GetMutableParticleIndices() { return ParticleIndices; }

	int32 GetParticleStride() const { return ParticleStride; }

	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	int32 ParticleStride = 0;
};
