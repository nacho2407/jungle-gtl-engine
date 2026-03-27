#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Resource/Buffer.h"

// Cooked Data 내부용 정점
struct FNormalVertex
{
	FVector pos;
	FVector normal;
	FVector4 color;
	FVector2 tex;
};

// Cooked Data — GPU용 정점/인덱스
struct FStaticMesh
{
	std::string PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	FMeshBuffer* RenderBuffer = nullptr;
};
