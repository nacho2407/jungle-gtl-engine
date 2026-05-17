#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/Fbx/FbxImportTypes.h"

struct FImportOptions;

class FFbxImporter
{
public:
	static bool ImportStaticMesh(
		const FString& FilePath,
		const FImportOptions* Options,
		FFbxStaticMeshImportResult& OutResult,
		FString* OutMessage = nullptr
	);

	static bool ImportSkeletalMesh(
		const FString& FilePath,
		FFbxSkeletalMeshImportResult& OutResult,
		FString* OutMessage = nullptr
	);

	static bool HasSkinDeformer(
		const FString& FilePath,
		FString* OutMessage = nullptr
	);
};
