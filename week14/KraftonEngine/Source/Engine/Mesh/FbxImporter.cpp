#include "FbxImporter.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxSceneLoader.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Fbx/FbxAnimationImporter.h"

#include <utility>

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

bool FFbxImporter::ImportSkeletalMeshOnly(const FString& FilePath, FFbxSkeletalMeshOnlyImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshOnlyImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);
	FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

	FFbxImportContext Context;
	Context.SourcePath = FilePath;
	return FFbxSkeletalMeshImporter::ImportMeshOnly(SceneHandle.Scene, Context, OutResult, OutMessage);
}

bool FFbxImporter::ImportAnimationOnly(const FString& FilePath, FFbxAnimationImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxAnimationImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);

	FbxNode* RootNode = SceneHandle.Scene ? SceneHandle.Scene->GetRootNode() : nullptr;
	if (!RootNode)
	{
		if (OutMessage) *OutMessage = "FBX animation import failed: root node not found.";
		return false;
	}

	FFbxImportContext Context;
	Context.SourcePath = FilePath;
	FFbxSceneQuery::CollectAllNodes(RootNode, Context.AllNodes);
	FFbxSceneQuery::CollectMeshNodes(RootNode, Context.MeshNodes);

	if (!FFbxSkeletonImporter::ImportSkeleton(SceneHandle.Scene, Context, OutMessage))
	{
		return false;
	}

	if (!FFbxAnimationImporter::ImportAnimations(SceneHandle.Scene, Context, OutMessage))
	{
		return false;
	}

	OutResult.SourceSkeleton = std::move(Context.ReferenceSkeleton);
	OutResult.AnimSequences  = std::move(Context.AnimSequences);
	return true;
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
