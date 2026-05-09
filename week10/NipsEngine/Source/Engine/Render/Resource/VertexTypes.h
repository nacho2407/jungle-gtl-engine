#pragma once

#include "Core/CoreMinimal.h"

/*
	Vertex 구조체들을 정의하는 Header입니다.
	추후에 다양한 Vertex 구조체들을 추가할 수 있습니다.
*/

struct FVertex
{ 
	FVector Position;
	FColor	Color;
	int		SubID;
};

struct FNormalVertex
{
	FVector		Position;
	FColor		Color;
	FVector		Normal;
	FVector2	UVs;	//	TexCoord
    FVector4	Tangent;
};

struct FSkeletalMeshVertex
{
    FVector Position;
    FVector Normal;
    FVector2 UVs;
    FVector4 Tangent;
    FColor Color;

    // Bone influence (핵심)
    uint8 BoneIndices[4];
    float BoneWeights[4];
};

struct FOverlayVertex
{
	float X, Y;
};

// Position + TexCoord 범용 버텍스 (FontBatcher, SubUVBatcher 등 텍스처 기반 배처 공용)
struct FTextureVertex
{
	FVector  Position;
	FVector2 TexCoord;
	FColor Color;
};

struct FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
};