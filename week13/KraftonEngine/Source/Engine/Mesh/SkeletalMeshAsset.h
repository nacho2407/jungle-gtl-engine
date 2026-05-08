#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"
#include "Math/Matrix.h"
#include "Serialization/Archive.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <memory>

struct FBone
{
	FString Name;
	int32 ParentIndex;

	FMatrix LocalMatrix;
	FMatrix GlobalMatrix;
	FMatrix InverseBindPoseMatrix;
};

struct FSkeletalMeshSection
{
	int32 MaterialIndex = -1;
	FString MaterialSlotName;
	uint32 FirstIndex;
	uint32 IndexCount;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshSection& Section)
	{
		Ar << Section.MaterialSlotName;
		Ar << Section.FirstIndex;
		Ar << Section.IndexCount;
		return Ar;
	}
};

struct FSkeletalMaterial
{
	UMaterial* MaterialInterface;
	FString MaterialSlotName = "None";

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMaterial& Mat)
	{
		Ar << Mat.MaterialSlotName;

		FString JsonPath;
		if (Ar.IsSaving() && Mat.MaterialInterface)
		{
			JsonPath = Mat.MaterialInterface->GetAssetPathFileName();
		}
		Ar << JsonPath;

		if (Ar.IsLoading())
		{
			if (!JsonPath.empty())
			{
				Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(JsonPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

		return Ar;
	}
};

struct FSkeletalMesh
{
	FString PathFileName;

	TArray<FVertexPNCTBW> Vertices;
	TArray<uint32> Indices;

	TArray<FSkeletalMeshSection> Sections;

	TArray<FBone> Bones;

	TMap<FString, int32> BoneNameToIndex;

	std::unique_ptr<FMeshBuffer> RenderBuffer;

	FVector BoundsCenter = FVector(0, 0, 0);
	FVector BoundsExtent = FVector(0, 0, 0);
	bool    bBoundsValid = false;

	void CacheBounds()
	{
		bBoundsValid = false;
		if (Vertices.empty()) return;

		FVector LocalMin = Vertices[0].Position;
		FVector LocalMax = Vertices[0].Position;
		for (const FVertexPNCTBW& Vertex : Vertices)
		{
			LocalMin.X = std::min(LocalMin.X, Vertex.Position.X);
			LocalMin.Y = std::min(LocalMin.Y, Vertex.Position.Y);
			LocalMin.Z = std::min(LocalMin.Z, Vertex.Position.Z);
			LocalMax.X = std::max(LocalMax.X, Vertex.Position.X);
			LocalMax.Y = std::max(LocalMax.Y, Vertex.Position.Y);
			LocalMax.Z = std::max(LocalMax.Z, Vertex.Position.Z);
		}

		BoundsCenter = (LocalMin + LocalMax) * 0.5f;
		BoundsExtent = (LocalMax - LocalMin) * 0.5f;
		bBoundsValid = true;
	}
};
