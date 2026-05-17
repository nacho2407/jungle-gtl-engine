#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxImportTypes.h"

#include <fbxsdk.h>

class FFbxSkeletalMeshImporter
{
public:
	static bool Import(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshImportResult& OutResult, FString* OutMessage = nullptr);
	static bool ImportMeshOnly(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshOnlyImportResult& OutResult, FString* OutMessage = nullptr);
};
