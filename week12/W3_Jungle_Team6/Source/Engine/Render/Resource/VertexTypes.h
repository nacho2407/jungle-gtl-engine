#pragma once

#include "Math/Vector.h"

/*
	Vertex 구조체들을 정의하는 Header입니다.
	추후에 다양한 Vertex 구조체들을 추가할 수 있습니다.
*/

struct FVertex
{ 
	FVector Position;
	FVector4 Color;
	int SubID;
};

struct FOverlayVertex
{
	float X, Y;
};


struct FFontVertex
{
	FVector  Position;   
	FVector2 TexCoord;
};

struct FMeshData
{
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
};