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

	uint32 FrameIndex = 0;
	bool bAlive = false;
};

struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;
};
