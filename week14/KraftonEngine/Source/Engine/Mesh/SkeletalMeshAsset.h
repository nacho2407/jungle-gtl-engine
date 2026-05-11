#pragma once

#include "Core/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"
#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Serialization/Archive.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <algorithm>
#include <memory>

inline void SerializeSkeletalMatrix(FArchive& Ar, FMatrix& Matrix)
{
	Ar.Serialize(Matrix.Data, sizeof(float) * 16);
}

inline void SerializeSkeletalTransform(FArchive& Ar, FTransform& Transform)
{
	Ar.Serialize(&Transform.Location, sizeof(FVector));
	Ar.Serialize(&Transform.Rotation, sizeof(FQuat));
	Ar.Serialize(&Transform.Scale, sizeof(FVector));
}

struct FBone
{
	FString Name;
	int32 ParentIndex;

	FTransform LocalTransform;
	FTransform GlobalTransform;
	FMatrix InverseBindPoseMatrix;

	friend FArchive& operator<<(FArchive& Ar, FBone& Bone)
	{
		Ar << Bone.Name;
		Ar << Bone.ParentIndex;
		SerializeSkeletalTransform(Ar, Bone.LocalTransform);
		SerializeSkeletalTransform(Ar, Bone.GlobalTransform);
		SerializeSkeletalMatrix(Ar, Bone.InverseBindPoseMatrix);
		return Ar;
	}
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
	UMaterial* MaterialInterface = nullptr;
	FString MaterialSlotName = "None";
	FString MaterialPath;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMaterial& Mat)
	{
		Ar << Mat.MaterialSlotName;

		// Material은 .bin에 직접 저장하지 않고 Asset/Materials/Auto/*.mat 경로만 저장한다.
		if (Ar.IsSaving() && Mat.MaterialInterface)
		{
			Mat.MaterialPath = Mat.MaterialInterface->GetAssetPathFileName();
		}
		Ar << Mat.MaterialPath;

		if (Ar.IsLoading())
		{
			if (!Mat.MaterialPath.empty())
			{
				Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(Mat.MaterialPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

		return Ar;
	}
};

struct FSkeletalMeshRange
{
	uint32 VertexStart = 0;
	uint32 VertexEnd = 0;
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	FMatrix MeshBindGlobal = FMatrix::Identity;

	friend FArchive& operator<<(FArchive& Ar, FSkeletalMeshRange& Range)
	{
		Ar << Range.VertexStart;
		Ar << Range.VertexEnd;
		Ar << Range.FirstIndex;
		Ar << Range.IndexCount;
		SerializeSkeletalMatrix(Ar, Range.MeshBindGlobal);
		return Ar;
	}
};

struct FSkeletalMesh
{
	FString PathFileName;

	TArray<FVertexPNCTBW> Vertices;
	TArray<uint32> Indices;

	TArray<FSkeletalMeshSection> Sections;
	TArray<FSkeletalMeshRange> MeshRanges;

	TArray<FBone> Bones;

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
			LocalMin.X = std::min<float>(LocalMin.X, Vertex.Position.X);
			LocalMin.Y = std::min<float>(LocalMin.Y, Vertex.Position.Y);
			LocalMin.Z = std::min<float>(LocalMin.Z, Vertex.Position.Z);
			LocalMax.X = std::max<float>(LocalMax.X, Vertex.Position.X);
			LocalMax.Y = std::max<float>(LocalMax.Y, Vertex.Position.Y);
			LocalMax.Z = std::max<float>(LocalMax.Z, Vertex.Position.Z);
		}

		BoundsCenter = (LocalMin + LocalMax) * 0.5f;
		BoundsExtent = (LocalMax - LocalMin) * 0.5f;
		bBoundsValid = true;
	}
};
