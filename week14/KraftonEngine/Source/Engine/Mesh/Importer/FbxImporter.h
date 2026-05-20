#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportTypes.h"

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

	static bool ImportSkeletalMeshOnly(const FString& FilePath, FFbxSkeletalMeshOnlyImportResult& OutResult, FString* OutMessage = nullptr);

	static bool ImportSkeletonOnly(
		const FString&            FilePath,
		FFbxSkeletonImportResult& OutResult,
		FString*                  OutMessage = nullptr
		);

	static bool ImportAnimationOnly(
		const FString&                    FilePath,
		FFbxAnimationImportResult&        OutResult,
		const FFbxAnimationImportOptions* Options    = nullptr,
		FString*                          OutMessage = nullptr
		);

	static bool ImportSkeletalScene(
		const FString&                        FilePath,
		const FFbxSkeletalSceneImportOptions& Options,
		FFbxSkeletalSceneImportResult&        OutResult,
		FString*                              OutMessage = nullptr
		);

	static bool ListAnimationStacks(
		const FString&                  FilePath,
		TArray<FFbxAnimationStackInfo>& OutStacks,
		FString*                        OutMessage = nullptr
		);

	static bool HasSkinDeformer(
		const FString& FilePath,
		FString* OutMessage = nullptr
	);
};
