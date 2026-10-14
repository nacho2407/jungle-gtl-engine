#pragma once

#include "Math/Vector.h"
#include "Math/Rotator.h"

enum class EPhysicsBodyType
{
	Static,
	Dynamic,
	Kinematic
};

struct FPhysicsBodyDesc
{
	EPhysicsBodyType BodyType = EPhysicsBodyType::Static;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Mass = 1.0f;
	bool bGravityEnabled = true;
	bool bTrigger = false;
};
