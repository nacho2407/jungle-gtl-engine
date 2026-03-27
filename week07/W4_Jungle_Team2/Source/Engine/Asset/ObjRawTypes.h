#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

/*
 *StaticMeshCookedData가 되기 이전인 Raw Data에 대한 정보
*/

struct FObjRawIndex
{
	int32 PositionIndex = -1;
	int32 UVIndex = -1;
	int32 NormalIndex = -1;
};

struct FObjRawFace
{
	TArray<FObjRawIndex> Vertices;
	FString MaterialName;
};

//	FObjInfo와 동일하지만 RawData라는 것을 드러냄
struct FObjRawData
{
	TArray<FVector> Positions;
	TArray<FObjRawFace> Faces;
	TArray<FVector> Normals;
	TArray<FVector2> TexCoords;	//	UVs
	
	FString ReferencedMtlPath;
};