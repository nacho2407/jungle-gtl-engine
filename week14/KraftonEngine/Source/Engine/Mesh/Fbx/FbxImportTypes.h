#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/StaticMeshAsset.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Animation/SkeletonTypes.h"

class UAnimSequence;

struct FFbxImportedMaterialInfo
{
	FString Name;
	FVector DiffuseColor = FVector(1.0f, 1.0f, 1.0f);
	FString DiffuseTexturePath;
	FString NormalTexturePath;
};

struct FFbxStaticMeshImportResult
{
	FStaticMesh Mesh;
	TArray<FStaticMaterial> Materials;
	TArray<FFbxImportedMaterialInfo> SourceMaterials;
};

struct FFbxSkeletalMeshImportResult
{
	FSkeletalMesh Mesh;
	TArray<FSkeletalMaterial> Materials;
	FReferenceSkeleton Skeleton;
	TArray<UAnimSequence*> AnimSequences;
	TArray<FFbxImportedMaterialInfo> SourceMaterials;
};

struct FFbxSkeletalMeshOnlyImportResult
{
	FSkeletalMesh                    Mesh;
	TArray<FSkeletalMaterial>        Materials;
	FReferenceSkeleton               SourceSkeleton;
	TArray<FFbxImportedMaterialInfo> SourceMaterials;
};

struct FFbxSkeletonImportResult
{
	FReferenceSkeleton SourceSkeleton;
};

struct FFbxAnimationStackInfo
{
	int32   StackIndex = -1;
	FString Name;
	double  StartSecond    = 0.0;
	double  EndSecond      = 0.0;
	double  DurationSecond = 0.0;
	int32   LayerCount     = 0;
};

struct FFbxAnimationImportOptions
{
	// Empty means import every stack. Filled means import only matching FBX AnimStack indices.
	TSet<int32> SelectedStackIndices;

	bool ShouldImportStack(int32 StackIndex) const
	{
		return SelectedStackIndices.empty() || SelectedStackIndices.find(StackIndex) != SelectedStackIndices.end();
	}
};

struct FFbxAnimationImportResult
{
	FReferenceSkeleton     SourceSkeleton;
	TArray<UAnimSequence*> AnimSequences;
};

struct FFbxSkeletalSceneImportOptions
{
	bool bImportSkeleton   = true;
	bool bImportSkin       = true;
	bool bImportAnimations = true;

	FFbxAnimationImportOptions AnimationOptions;
};

struct FFbxSkeletalSceneImportResult
{
	FReferenceSkeleton               SourceSkeleton;
	FSkeletalMesh                    Mesh;
	TArray<FSkeletalMaterial>        Materials;
	TArray<UAnimSequence*>           AnimSequences;
	TArray<FFbxImportedMaterialInfo> SourceMaterials;

	bool bHasSkeleton = false;
	bool bHasMesh     = false;
};
