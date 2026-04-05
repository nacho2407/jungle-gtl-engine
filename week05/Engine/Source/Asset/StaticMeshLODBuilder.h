#pragma once
#include "CoreMinimal.h"

class UStaticMesh;

struct ENGINE_API FStaticMeshLODSettings
{
	int32 NumLODs = 3;
	float TriangleReductionStep = 0.3f;
	float DistanceStep = 10.0f;
};

class ENGINE_API FStaticMeshLODBuilder
{
public:
	static void BuildLODs(UStaticMesh& Asset, const FStaticMeshLODSettings& Settings = {});
};
