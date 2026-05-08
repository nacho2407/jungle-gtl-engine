#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Math/Matrix.h"

struct FBone
{
	FString Name;
	int32 ParentIndex;

	FMatrix LocalMatrix;
};

struct FSkeletalMesh
{
	FString PathFileName;

	TArray<FVertexPNCTBW> Vertices;
	TArray<uint32> Indices;

	TArray<FBone> Bones;

	TMap<FString, int32> BoneNameToIndex;
};
