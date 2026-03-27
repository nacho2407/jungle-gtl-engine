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

	bool operator==(const FObjRawIndex& Other) const
	{
		return PositionIndex == Other.PositionIndex && UVIndex == Other.UVIndex && NormalIndex == Other.NormalIndex;
	}
	
	bool operator<(const FObjRawIndex& Other) const
	{
		if (PositionIndex != Other.PositionIndex) return PositionIndex < Other.PositionIndex;
		if (UVIndex != Other.UVIndex) return UVIndex < Other.UVIndex;
		return NormalIndex < Other.NormalIndex;
	}
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
	TArray<FVector2> UVs; //	TexCoords

	FString ReferencedMtlPath;
};

/* 
 * OBJ File에서의 pos, uv, normal index를 key로 매핑하여 동일한 vertex에 대해 재사용성을 높여줍니다.
 * 이렇게 하지 않으면 Index Buffer의 의미가 없습니다.
 */

struct FObjVertexKey
{
	FObjRawIndex ObjRawIndex;

	bool operator==(const FObjVertexKey& Other) const
	{
		return ObjRawIndex == Other.ObjRawIndex;
	}

	//	For Map
	bool operator<(const FObjVertexKey& Other) const
	{
		return ObjRawIndex < Other.ObjRawIndex;
	}
};
