#include "Mesh/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxMaterialImporter.h"
#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Fbx/FbxSkinWeightImporter.h"
#include "Mesh/Fbx/FbxAnimationImporter.h"

#include <utility>

bool FFbxSkeletalMeshImporter::Import(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshImportResult();

	FbxNode* RootNode = Scene ? Scene->GetRootNode() : nullptr;
	if (!RootNode)
	{
		if (OutMessage) *OutMessage = "FBX skeletal mesh import failed: root node not found.";
		return false;
	}

	Context.AllNodes.clear();
	Context.MeshNodes.clear();
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
		RefBone.Name = Bone.Name;
		RefBone.ParentIndex = Bone.ParentIndex;
		RefBone.LocalBindPose = Bone.LocalMatrix;
		RefBone.GlobalBindPose = Bone.GlobalMatrix;
		RefBone.InverseBindPose = Bone.InverseBindPoseMatrix;
		Context.ReferenceSkeleton.Bones.push_back(RefBone);
	}

	if (!FFbxAnimationImporter::ImportAnimations(Scene, Context, OutMessage))
	{
		return false;
	}

	OutResult.Mesh.Vertices = std::move(Context.SkeletalVertices);
	OutResult.Mesh.Indices = std::move(Context.SkeletalIndices);
	OutResult.Mesh.Sections = std::move(Context.SkeletalSections);
	OutResult.Mesh.MeshRanges = std::move(Context.SkeletalMeshRanges);
	OutResult.Mesh.Bones = std::move(Context.Bones);
	OutResult.Mesh.PathFileName = Context.SourcePath;

	OutResult.Skeleton = std::move(Context.ReferenceSkeleton);
	OutResult.AnimSequences = std::move(Context.AnimSequences);
	OutResult.SourceMaterials = Context.Materials;
	FFbxMaterialImporter::BuildSkeletalMaterials(Context, OutResult.Mesh.Sections, OutResult.Materials, OutResult.Mesh.Sections);

	return true;
}
