#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxMaterialImporter.h"
#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Fbx/FbxSkinWeightImporter.h"
#include "Mesh/Fbx/FbxAnimationImporter.h"

#include <utility>

namespace
{
	static bool ImportMeshCore(
		FbxScene*                         Scene,
		FFbxImportContext&                Context,
		FSkeletalMesh&                    OutMesh,
		TArray<FSkeletalMaterial>&        OutMaterials,
		FReferenceSkeleton&               OutSourceSkeleton,
		TArray<FFbxImportedMaterialInfo>& OutSourceMaterials,
		FString*                          OutMessage
		)
	{
		FbxNode* RootNode = Scene ? Scene->GetRootNode() : nullptr;
		if (!RootNode)
		{
			if (OutMessage) *OutMessage = "FBX skeletal mesh import failed: root node not found.";
			return false;
		}

		Context.AllNodes.clear();
		Context.MeshNodes.clear();
		Context.AnimSequences.clear();
		FFbxSceneQuery::CollectAllNodes(RootNode, Context.AllNodes);
		FFbxSceneQuery::CollectMeshNodes(RootNode, Context.MeshNodes);

		FFbxMaterialImporter::CollectMaterials(Scene, Context);

		if (!FFbxSkeletonImporter::ImportSkeleton(Scene, Context, OutMessage))
		{
			return false;
		}

		if (!FFbxSkinWeightImporter::ImportSkin(Scene, Context, OutMessage))
		{
			return false;
		}

		// Skin import can refine inverse bind poses from FBX clusters, so rebuild the reference skeleton after skin data is processed.
		Context.ReferenceSkeleton.Bones.clear();
		Context.ReferenceSkeleton.Bones.reserve(Context.Bones.size());
		for (const FBone& Bone : Context.Bones)
		{
			FReferenceBone RefBone;
			RefBone.Name            = Bone.Name;
			RefBone.ParentIndex     = Bone.ParentIndex;
			RefBone.LocalBindPose   = Bone.LocalMatrix;
			RefBone.GlobalBindPose  = Bone.GlobalMatrix;
			RefBone.InverseBindPose = Bone.InverseBindPoseMatrix;
			Context.ReferenceSkeleton.Bones.push_back(RefBone);
		}

		OutMesh.Vertices     = std::move(Context.SkeletalVertices);
		OutMesh.Indices      = std::move(Context.SkeletalIndices);
		OutMesh.Sections     = std::move(Context.SkeletalSections);
		OutMesh.MeshRanges   = std::move(Context.SkeletalMeshRanges);
		OutMesh.Bones        = Context.Bones;
		OutMesh.PathFileName = Context.SourcePath;

		OutSourceSkeleton  = Context.ReferenceSkeleton;
		OutSourceMaterials = Context.Materials;
		FFbxMaterialImporter::BuildSkeletalMaterials(Context, OutMesh.Sections, OutMaterials, OutMesh.Sections);
		return true;
	}
}

bool FFbxSkeletalMeshImporter::Import(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshImportResult();

	if (!ImportMeshCore(Scene, Context, OutResult.Mesh, OutResult.Materials, OutResult.Skeleton, OutResult.SourceMaterials, OutMessage))
	{
		return false;
	}

	if (!FFbxAnimationImporter::ImportAnimations(Scene, Context, nullptr, OutMessage))
	{
		return false;
	}

	OutResult.AnimSequences = std::move(Context.AnimSequences);
	return true;
}

bool FFbxSkeletalMeshImporter::ImportMeshOnly(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshOnlyImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshOnlyImportResult();
	return ImportMeshCore(Scene, Context, OutResult.Mesh, OutResult.Materials, OutResult.SourceSkeleton, OutResult.SourceMaterials, OutMessage);
}
