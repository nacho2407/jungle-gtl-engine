#pragma once

#include "Core/Types/CoreTypes.h"
#include "Engine/Math/Vector.h"

struct FWireLine
{
	FVector Start;
	FVector End;
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

	void Reset()
	{
		Vertices.clear();
		Indices.clear();
	}

	bool IsValid() const
	{
		return !Vertices.empty() && !Indices.empty();
	}
};
