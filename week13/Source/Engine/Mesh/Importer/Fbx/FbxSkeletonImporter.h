#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"

#include <fbxsdk.h>

class FFbxSkeletonImporter
{
public:
	static bool ImportSkeleton(FbxScene* Scene, FFbxImportContext& Context, FString* OutMessage = nullptr);
	static int32 FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& NodeToIndex);
};
