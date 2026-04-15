#pragma once

#include "Math/Vector.h"
#include "Render/Types/RenderTypes.h"

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

// Position + TexCoord 범용 버텍스 (FFontGeometry 등 텍스처 기반 동적 지오메트리 공용)
struct FTextureVertex
{
	FVector  Position;
	FVector2 TexCoord;
};

// Position + Normal + Color + UV (StaticMesh GPU용 정점 형식)
struct FVertexPNCT
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 UV;
};

template<typename VertexType>
struct TMeshData
{
	TArray<VertexType> Vertices;
	TArray<uint32> Indices;
};

using FMeshData = TMeshData<FVertex>;
