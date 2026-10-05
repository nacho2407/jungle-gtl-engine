#pragma once

#include "Core/CoreMinimal.h"
#include "Math/Vector.h"

#include <functional>

/*
 *	StaticMeshCookedData가 되기 이전인 Raw Data에 대한 정보
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
 * OBJ File에서의 pos, uv, normal index를 key로 매핑하여 동일한 vertex에 대해 재사용성을 높여줌
 * 이렇게 하지 않으면 Index Buffer의 의미가 없음
 */


struct FObjVertexKey
{
	FObjRawIndex ObjRawIndex;

	bool operator==(const FObjVertexKey& Other) const
	{
		return ObjRawIndex == Other.ObjRawIndex;
	}
};


namespace std
{
	template<>
	struct hash<FObjRawIndex>
	{
		size_t operator()(const FObjRawIndex& Key) const noexcept
		{
			size_t h1 = std::hash<int32>()(Key.PositionIndex);
			size_t h2 = std::hash<int32>()(Key.UVIndex);
			size_t h3 = std::hash<int32>()(Key.NormalIndex);

			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

	template<>
	struct hash<FObjVertexKey>
	{
		size_t operator()(const FObjVertexKey& Key) const noexcept
		{
			return std::hash<FObjRawIndex>()(Key.ObjRawIndex);
		}
	};
}