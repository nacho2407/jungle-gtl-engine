#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"
#include "Mesh/Importer/Fbx/FbxImportTypes.h"

#include <fbxsdk.h>

class FFbxSkeletalMeshImporter
{
public:
	static bool Import(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshImportResult& OutResult, FString* OutMessage = nullptr);
	static bool ImportMeshOnly(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshOnlyImportResult& OutResult, FString* OutMessage = nullptr);
};
