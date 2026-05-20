#include "FbxImporter.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"
#include "Mesh/Importer/Fbx/FbxSceneLoader.h"
#include "Mesh/Importer/Fbx/FbxSceneQuery.h"
#include "Mesh/Importer/Fbx/FbxStaticMeshImporter.h"
#include "Mesh/Importer/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Importer/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Importer/Fbx/FbxAnimationImporter.h"
#include "Platform/Paths.h"

#include <utility>

namespace
{
	static bool TryResolveAnimationTimeRange(const FbxTakeInfo* TakeInfo, double& OutStartSecond, double& OutEndSecond)
	{
		if (!TakeInfo)
		{
			return false;
		}

		auto TrySpan = [&](const FbxTimeSpan& Span) -> bool
		{
			const double Start = Span.GetStart().GetSecondDouble();
			const double End   = Span.GetStop().GetSecondDouble();
			if (End <= Start)
			{
				return false;
			}
			OutStartSecond = Start;
			OutEndSecond   = End;
			return true;
		};

		return TrySpan(TakeInfo->mLocalTimeSpan) || TrySpan(TakeInfo->mReferenceTimeSpan);
	}

	static FFbxSceneLoadOptions MakeStaticMeshLoadOptions()
	{
		FFbxSceneLoadOptions Options;
		Options.bImportLinks      = false;
		Options.bImportAnimations = false;
		Options.bImportGobos      = false;
		return Options;
	}

	static FFbxSceneLoadOptions MakeSkeletalMeshLoadOptions(bool bImportAnimation)
	{
		FFbxSceneLoadOptions Options;
		Options.bImportAnimations = bImportAnimation;
		Options.bImportGobos      = false;
		return Options;
	}

	static FFbxSceneLoadOptions MakeSkeletonOnlyLoadOptions()
	{
		FFbxSceneLoadOptions Options;
		Options.bImportMaterials  = false;
		Options.bImportTextures   = false;
		Options.bImportLinks      = false;
		Options.bImportShapes     = false;
		Options.bImportGobos      = false;
		Options.bImportAnimations = false;
		return Options;
	}

	static FFbxSceneLoadOptions MakeAnimationOnlyLoadOptions(const FFbxAnimationImportOptions* AnimationOptions)
	{
		FFbxSceneLoadOptions Options;
		Options.bImportMaterials  = false;
		Options.bImportTextures   = false;
		Options.bImportLinks      = false;
		Options.bImportShapes     = false;
		Options.bImportGobos      = false;
		Options.bImportAnimations = true;
		if (AnimationOptions)
		{
			Options.SelectedAnimationStackIndices = AnimationOptions->SelectedStackIndices;
		}
		return Options;
	}

	static FFbxSceneLoadOptions MakeSkinProbeLoadOptions()
	{
		FFbxSceneLoadOptions Options;
		Options.bImportMaterials  = false;
		Options.bImportTextures   = false;
		Options.bImportShapes     = false;
		Options.bImportGobos      = false;
		Options.bImportAnimations = false;
		return Options;
	}

	static FFbxSceneLoadOptions MakeSkeletalSceneLoadOptions(const FFbxSkeletalSceneImportOptions& ImportOptions)
	{
		FFbxSceneLoadOptions Options;

		// Skeleton/animation-only imports do not need material, texture, shape, or gobo payloads.
		Options.bImportMaterials  = ImportOptions.bImportSkin;
		Options.bImportTextures   = ImportOptions.bImportSkin;
		Options.bImportLinks      = ImportOptions.bImportSkin;
		Options.bImportShapes     = ImportOptions.bImportSkin;
		Options.bImportGobos      = false;
		Options.bImportAnimations = ImportOptions.bImportAnimations;

		if (ImportOptions.bImportAnimations)
		{
			Options.SelectedAnimationStackIndices = ImportOptions.AnimationOptions.SelectedStackIndices;
		}

		return Options;
	}
}

bool FFbxImporter::ImportStaticMesh(const FString& FilePath, const FImportOptions* Options, FFbxStaticMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxStaticMeshImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, MakeStaticMeshLoadOptions(), SceneHandle, OutMessage))
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
	if (!FFbxSceneLoader::Load(FilePath, MakeSkeletalMeshLoadOptions(true), SceneHandle, OutMessage))
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
	if (!FFbxSceneLoader::Load(FilePath, MakeSkeletalMeshLoadOptions(false), SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);
	FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);

	FFbxImportContext Context;
	Context.SourcePath = FilePath;
	return FFbxSkeletalMeshImporter::ImportMeshOnly(SceneHandle.Scene, Context, OutResult, OutMessage);
}

bool FFbxImporter::ImportSkeletonOnly(const FString& FilePath, FFbxSkeletonImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletonImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, MakeSkeletonOnlyLoadOptions(), SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);

	FbxNode* RootNode = SceneHandle.Scene ? SceneHandle.Scene->GetRootNode() : nullptr;
	if (!RootNode)
	{
		if (OutMessage) *OutMessage = "FBX skeleton import failed: root node not found.";
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

	OutResult.SourceSkeleton = std::move(Context.ReferenceSkeleton);
	return true;
}

bool FFbxImporter::ImportAnimationOnly(
	const FString&                    FilePath,
	FFbxAnimationImportResult&        OutResult,
	const FFbxAnimationImportOptions* Options,
	FString*                          OutMessage
	)
{
	OutResult = FFbxAnimationImportResult();

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, MakeAnimationOnlyLoadOptions(Options), SceneHandle, OutMessage))
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

	if (!FFbxAnimationImporter::ImportAnimations(SceneHandle.Scene, Context, Options, OutMessage))
	{
		return false;
	}

	OutResult.SourceSkeleton = std::move(Context.ReferenceSkeleton);
	OutResult.AnimSequences  = std::move(Context.AnimSequences);
	return true;
}

bool FFbxImporter::ImportSkeletalScene(
	const FString&                        FilePath,
	const FFbxSkeletalSceneImportOptions& Options,
	FFbxSkeletalSceneImportResult&        OutResult,
	FString*                              OutMessage
	)
{
	OutResult = FFbxSkeletalSceneImportResult();

	if (!Options.bImportSkeleton && !Options.bImportSkin && !Options.bImportAnimations)
	{
		if (OutMessage) *OutMessage = "FBX skeletal scene import failed: no import part was selected.";
		return false;
	}

	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, MakeSkeletalSceneLoadOptions(Options), SceneHandle, OutMessage))
	{
		return false;
	}

	FFbxSceneLoader::NormalizeScene(SceneHandle.Scene);
	if (Options.bImportSkin)
	{
		FFbxSceneLoader::Triangulate(SceneHandle.Manager, SceneHandle.Scene);
	}

	FbxNode* RootNode = SceneHandle.Scene ? SceneHandle.Scene->GetRootNode() : nullptr;
	if (!RootNode)
	{
		if (OutMessage) *OutMessage = "FBX skeletal scene import failed: root node not found.";
		return false;
	}

	FFbxImportContext Context;
	Context.SourcePath = FilePath;

	if (Options.bImportSkin)
	{
		FFbxSkeletalMeshOnlyImportResult MeshResult;
		if (!FFbxSkeletalMeshImporter::ImportMeshOnly(SceneHandle.Scene, Context, MeshResult, OutMessage))
		{
			return false;
		}

		OutResult.SourceSkeleton  = MeshResult.SourceSkeleton;
		OutResult.Mesh            = std::move(MeshResult.Mesh);
		OutResult.Materials       = std::move(MeshResult.Materials);
		OutResult.SourceMaterials = std::move(MeshResult.SourceMaterials);
		OutResult.bHasSkeleton    = OutResult.SourceSkeleton.GetNumBones() > 0;
		OutResult.bHasMesh        = true;
	}
	else
	{
		FFbxSceneQuery::CollectAllNodes(RootNode, Context.AllNodes);

		if (!FFbxSkeletonImporter::ImportSkeleton(SceneHandle.Scene, Context, OutMessage))
		{
			return false;
		}

		OutResult.SourceSkeleton = Context.ReferenceSkeleton;
		OutResult.bHasSkeleton   = OutResult.SourceSkeleton.GetNumBones() > 0;
	}

	if (Options.bImportAnimations)
	{
		if (!FFbxAnimationImporter::ImportAnimations(SceneHandle.Scene, Context, &Options.AnimationOptions, OutMessage))
		{
			return false;
		}

		OutResult.AnimSequences = std::move(Context.AnimSequences);
	}

	return true;
}

bool FFbxImporter::ListAnimationStacks(
	const FString&                  FilePath,
	TArray<FFbxAnimationStackInfo>& OutStacks,
	FString*                        OutMessage
	)
{
	OutStacks.clear();

	FbxManager* SdkManager = FbxManager::Create();
	if (!SdkManager)
	{
		if (OutMessage) *OutMessage = "FBX SDK manager creation failed.";
		return false;
	}

	FbxIOSettings* IoSettings = FbxIOSettings::Create(SdkManager, IOSROOT);
	if (!IoSettings)
	{
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = "FBX IO settings creation failed.";
		return false;
	}
	SdkManager->SetIOSettings(IoSettings);

	FbxImporter* Importer = FbxImporter::Create(SdkManager, "");
	if (!Importer)
	{
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = "FBX importer creation failed.";
		return false;
	}

	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(FilePath)));
	if (!Importer->Initialize(FullPath.c_str(), -1, SdkManager->GetIOSettings()))
	{
		Importer->Destroy();
		SdkManager->Destroy();
		if (OutMessage) *OutMessage = FString("FBX importer initialize failed: ") + FilePath;
		return false;
	}

	const int32 AnimStackCount = Importer->GetAnimStackCount();
	OutStacks.reserve(AnimStackCount);

	for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
	{
		FbxTakeInfo* TakeInfo = Importer->GetTakeInfo(StackIndex);
		if (!TakeInfo)
		{
			continue;
		}

		FFbxAnimationStackInfo Info;
		Info.StackIndex = StackIndex;
		Info.Name       = TakeInfo->mName.Buffer() ? FString(TakeInfo->mName.Buffer()) : FString("Anim");
		Info.LayerCount = -1;

		double Start = 0.0;
		double End   = 0.0;
		if (TryResolveAnimationTimeRange(TakeInfo, Start, End))
		{
			Info.StartSecond    = Start;
			Info.EndSecond      = End;
			Info.DurationSecond = End - Start;
		}

		OutStacks.push_back(std::move(Info));
	}

	Importer->Destroy();
	SdkManager->Destroy();
	return true;
}

bool FFbxImporter::HasSkinDeformer(const FString& FilePath, FString* OutMessage)
{
	FFbxSceneHandle SceneHandle;
	if (!FFbxSceneLoader::Load(FilePath, MakeSkinProbeLoadOptions(), SceneHandle, OutMessage))
	{
		return false;
	}

	return FFbxSceneQuery::SceneHasSkinDeformer(SceneHandle.Scene);
}
