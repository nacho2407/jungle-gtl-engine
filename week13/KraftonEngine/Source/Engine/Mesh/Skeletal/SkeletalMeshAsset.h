#pragma once

#include "Core/Types/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"
#include "Math/Matrix.h"
#include "Serialization/Archive.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <algorithm>
#include <memory>

inline void SerializeSkeletalMatrix(FArchive& Ar, FMatrix& Matrix)
{
	Ar.Serialize(Matrix.Data, sizeof(float) * 16);
}

struct FBone
{
	FString Name;
	int32 ParentIndex = -1;

	// Runtime pose semantics. The legacy LocalMatrix/GlobalMatrix slots are kept
	// for existing package layout, but code should use these named poses.
	FMatrix ReferenceLocalPose = FMatrix::Identity;
	FMatrix ReferenceGlobalPose = FMatrix::Identity;
	FMatrix SkinBindGlobalPose = FMatrix::Identity;
	FMatrix LocalMatrix = FMatrix::Identity;
	FMatrix GlobalMatrix = FMatrix::Identity;
	FMatrix InverseBindPoseMatrix = FMatrix::Identity;

	void SyncSeparatedPoseDataFromLegacy()
	{
		ReferenceLocalPose = LocalMatrix;
		ReferenceGlobalPose = GlobalMatrix;
		SkinBindGlobalPose = GlobalMatrix;
	}

	void SyncLegacyPoseDataFromSeparated()
	{
		LocalMatrix = ReferenceLocalPose;
		GlobalMatrix = SkinBindGlobalPose;
	}

	const FMatrix& GetReferenceLocalPose() const { return ReferenceLocalPose; }
	const FMatrix& GetReferenceGlobalPose() const { return ReferenceGlobalPose; }
	const FMatrix& GetSkinBindGlobalPose() const { return SkinBindGlobalPose; }
	const FMatrix& GetInverseBindPose() const { return InverseBindPoseMatrix; }

	friend FArchive& operator<<(FArchive& Ar, FBone& Bone)
	{
		if (Ar.IsSaving())
		{
			Bone.SyncLegacyPoseDataFromSeparated();
		}

		Ar << Bone.Name;
		Ar << Bone.ParentIndex;
		SerializeSkeletalMatrix(Ar, Bone.LocalMatrix);
		SerializeSkeletalMatrix(Ar, Bone.GlobalMatrix);
		SerializeSkeletalMatrix(Ar, Bone.InverseBindPoseMatrix);

		if (Ar.IsLoading())
		{
			Bone.SyncSeparatedPoseDataFromLegacy();
		}
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

		// Material 포인터는 실행마다 달라질 수 있다.
		// .sketbin에는 다시 찾을 수 있는 .mat 경로만 저장한다.
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
	// Legacy serialization slot. New imports bake mesh bind transforms into vertices.
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

struct FMorphTargetDelta
{
	uint32  VertexIndex   = 0;
	FVector PositionDelta = FVector::ZeroVector;

	friend FArchive& operator<<(FArchive& Ar, FMorphTargetDelta& Delta)
	{
		Ar << Delta.VertexIndex;
		Ar << Delta.PositionDelta;
		return Ar;
	}
};

struct FMorphTarget
{
	FString                   Name;
	TArray<FMorphTargetDelta> Deltas;

	friend FArchive& operator<<(FArchive& Ar, FMorphTarget& Target)
	{
		Ar << Target.Name;
		Ar << Target.Deltas;
		return Ar;
	}
};

struct FSkeletalMesh
{
	FString PathFileName;
	FString SkeletonPath = "None";
	FString SkeletonAssetGuid;
	FString SkeletonCompatibilitySignature;

	TArray<FVertexPNCTBW> Vertices;
	TArray<uint32> Indices;

	TArray<FSkeletalMeshSection> Sections;
	TArray<FSkeletalMeshRange> MeshRanges;

	TArray<FBone>        Bones;
	TArray<FMorphTarget> MorphTargets;

	std::unique_ptr<FMeshBuffer> RenderBuffer;

	FVector BoundsCenter = FVector(0, 0, 0);
	FVector BoundsExtent = FVector(0, 0, 0);
	bool    bBoundsValid = false;

	int32 FindMorphTargetIndex(const FString& TargetName) const
	{
		for (int32 Index = 0; Index < static_cast<int32>(MorphTargets.size()); ++Index)
		{
			if (MorphTargets[Index].Name == TargetName)
			{
				return Index;
			}
		}
		return -1;
	}

	const FMorphTarget* FindMorphTarget(const FString& TargetName) const
	{
		const int32 Index = FindMorphTargetIndex(TargetName);
		return Index >= 0 ? &MorphTargets[Index] : nullptr;
	}

	void NormalizeBonePoseData()
	{
		for (FBone& Bone : Bones)
		{
			Bone.ReferenceLocalPose = Bone.LocalMatrix;
			Bone.SkinBindGlobalPose = Bone.GlobalMatrix;
		}

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
		{
			FBone& Bone = Bones[BoneIndex];
			Bone.ReferenceGlobalPose = (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
				? Bone.ReferenceLocalPose * Bones[Bone.ParentIndex].ReferenceGlobalPose
				: Bone.ReferenceLocalPose;
		}
	}

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
