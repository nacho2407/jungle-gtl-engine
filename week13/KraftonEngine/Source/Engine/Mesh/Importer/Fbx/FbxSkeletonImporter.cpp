#include "Mesh/Importer/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Importer/Fbx/FbxSceneQuery.h"
#include "Mesh/Importer/Fbx/FbxTransformUtils.h"

namespace
{
	static void BuildReferenceSkeleton(FFbxImportContext& Context)
	{
		Context.ReferenceSkeleton.Bones.clear();
		Context.ReferenceSkeleton.Bones.reserve(Context.Bones.size());

		for (const FBone& Bone : Context.Bones)
		{
			FReferenceBone RefBone;
			RefBone.Name = Bone.Name;
			RefBone.ParentIndex = Bone.ParentIndex;
			RefBone.LocalBindPose = Bone.GetReferenceLocalPose();
			RefBone.GlobalBindPose = Bone.GetReferenceGlobalPose();
			RefBone.InverseBindPose = Bone.GetInverseBindPose();
			Context.ReferenceSkeleton.Bones.push_back(RefBone);
		}
	}
}

bool FFbxSkeletonImporter::ImportSkeleton(FbxScene* Scene, FFbxImportContext& Context, FString* OutMessage)
{
	(void)Scene;
	Context.Bones.clear();
	Context.BoneNodeToIndex.clear();
	Context.ReferenceSkeleton.Bones.clear();

	for (FbxNode* Node : Context.AllNodes)
	{
		if (!FFbxSceneQuery::IsSkeletonNode(Node))
		{
			continue;
		}

		FBone Bone;
		Bone.Name = Node->GetName();

		Bone.ParentIndex = FindNearestParentBoneIndex(Node, Context.BoneNodeToIndex);

		const FbxAMatrix GlobalFbxMatrix = Node->EvaluateGlobalTransform();
		const bool bAbsorbWrapperTransform = Bone.ParentIndex < 0 && FFbxSceneQuery::HasNonSkeletonWrapperParent(Node);
		const FbxAMatrix LocalFbxMatrix = bAbsorbWrapperTransform
			? GlobalFbxMatrix
			: Node->EvaluateLocalTransform();
		Bone.LocalMatrix = FFbxTransformUtils::ToEngineMatrix(LocalFbxMatrix);
		Bone.GlobalMatrix = FFbxTransformUtils::ToEngineMatrix(GlobalFbxMatrix);
		Bone.InverseBindPoseMatrix = FFbxTransformUtils::ToEngineInverseMatrix(GlobalFbxMatrix);
		Bone.SyncSeparatedPoseDataFromLegacy();

		const int32 NewBoneIndex = static_cast<int32>(Context.Bones.size());
		Context.Bones.push_back(Bone);
		Context.BoneNodeToIndex[Node] = NewBoneIndex;
	}

	if (Context.Bones.empty())
	{
		if (OutMessage) *OutMessage = "FBX skeletal import failed: no skeleton nodes found.";
		return false;
	}

	BuildReferenceSkeleton(Context);
	return true;
}

int32 FFbxSkeletonImporter::FindNearestParentBoneIndex(FbxNode* Node, const TMap<FbxNode*, int32>& NodeToIndex)
{
	FbxNode* Parent = Node ? Node->GetParent() : nullptr;
	while (Parent)
	{
		auto It = NodeToIndex.find(Parent);
		if (It != NodeToIndex.end())
		{
			return It->second;
		}

		Parent = Parent->GetParent();
	}

	return -1;
}
