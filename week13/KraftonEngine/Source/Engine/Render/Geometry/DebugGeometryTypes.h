#pragma once

#include "Core/Types/CoreTypes.h"
#include "Engine/Math/Vector.h"

struct FWireLine
{
	FVector Start;
	FVector End;
};

struct FPhysicsDebugLine
{
	FVector Start;
	FVector End;
	FVector4 Color;
};

struct FPhysicsDebugVertex
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
};

struct FPhysicsDebugSolidMesh
{
	TArray<FPhysicsDebugVertex> Vertices;
	TArray<uint32> Indices;
	uint64 Revision = 0;

	void Reset()
	{
		Vertices.clear();
		Indices.clear();
		Revision = 0;
	}

	bool IsValid() const
	{
		return !Vertices.empty() && !Indices.empty();
	}
};
