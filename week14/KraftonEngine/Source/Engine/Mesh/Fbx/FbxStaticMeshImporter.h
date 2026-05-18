#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxImportTypes.h"

#include <fbxsdk.h>

struct FImportOptions;

class FFbxStaticMeshImporter
{
public:
	static bool Import(FbxScene* Scene, const FString& SourcePath, const FImportOptions* Options, FFbxImportContext& Context, FFbxStaticMeshImportResult& OutResult, FString* OutMessage = nullptr);
};
