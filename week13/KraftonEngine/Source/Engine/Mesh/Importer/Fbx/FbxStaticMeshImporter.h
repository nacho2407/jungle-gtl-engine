#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"
#include "Mesh/Importer/Fbx/FbxImportTypes.h"

#include <fbxsdk.h>

struct FImportOptions;

class FFbxStaticMeshImporter
{
public:
	static bool Import(FbxScene* Scene, const FString& SourcePath, const FImportOptions* Options, FFbxImportContext& Context, FFbxStaticMeshImportResult& OutResult, FString* OutMessage = nullptr);
};
