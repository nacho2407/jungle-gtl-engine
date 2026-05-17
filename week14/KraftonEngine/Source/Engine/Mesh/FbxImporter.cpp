#include "FbxImporter.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxSceneLoader.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"

bool FFbxImporter::ImportStaticMesh(const FString& FilePath, const FImportOptions* Options, FFbxStaticMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxStaticMeshImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);
	FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

	FFbxImportContext Context;
	Context.SourcePath = FilePath;
	return FFbxStaticMeshImporter::Import(SceneHandle.Scene, FilePath, Options, Context, OutResult, OutMessage);
}

bool FFbxImporter::ImportSkeletalMesh(const FString& FilePath, FFbxSkeletalMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);
	FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

	FFbxImportContext Context;
	Context.SourcePath = FilePath;
	return FFbxSkeletalMeshImporter::Import(SceneHandle.Scene, Context, OutResult, OutMessage);
}

bool FFbxImporter::HasSkinDeformer(const FString& FilePath, FString* OutMessage)
{
	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, SceneHandle, OutMessage))
	{
		return false;
	}

	return FFbxSceneQuery::SceneHasSkinDeformer(SceneHandle.Scene);
}
