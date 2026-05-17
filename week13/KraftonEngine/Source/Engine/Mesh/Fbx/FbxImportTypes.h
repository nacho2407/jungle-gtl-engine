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

struct FFbxAnimationImportResult
{
	FReferenceSkeleton     SourceSkeleton;
	TArray<UAnimSequence*> AnimSequences;
};
