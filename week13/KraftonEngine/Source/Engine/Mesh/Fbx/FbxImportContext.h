#pragma once

#include "Mesh/Fbx/FbxImportTypes.h"
#include "Render/Types/VertexTypes.h"

#include <fbxsdk.h>

struct FFbxImportContext
{
	FString SourcePath;

	TArray<FbxNode*> AllNodes;
	TArray<FbxNode*> MeshNodes;

	TArray<FFbxImportedMaterialInfo> Materials;
	TMap<FbxSurfaceMaterial*, int32> MaterialToSlotIndex;

	TArray<FBone> Bones;
	TMap<FbxNode*, int32> BoneNodeToIndex;
	FReferenceSkeleton ReferenceSkeleton;

	TArray<FVertexPNCTBW> SkeletalVertices;
	TArray<uint32> SkeletalIndices;
	TArray<FSkeletalMeshSection> SkeletalSections;
	TArray<FSkeletalMeshRange> SkeletalMeshRanges;

	TArray<FVector> TangentSums;
	TArray<FVector> BitangentSums;

	TArray<UAnimSequence*> AnimSequences;
};
