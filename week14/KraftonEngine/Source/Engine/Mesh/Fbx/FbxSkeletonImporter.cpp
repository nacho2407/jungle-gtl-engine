#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxTransformUtils.h"

namespace
{
	static int32 AddSyntheticRootBoneIfNeeded(FbxNode* Node, FFbxImportContext& Context)
	{
		if (!Node || !Node->GetParent() || FFbxSceneQuery::IsSkeletonNode(Node))
		{
			return -1;
		}

		auto Existing = Context.BoneNodeToIndex.find(Node);
		if (Existing != Context.BoneNodeToIndex.end())
		{
			return Existing->second;
		}

		FBone Bone;
		Bone.Name = Node->GetName();
		Bone.ParentIndex = -1;

		FbxNode* Parent = Node->GetParent();
		while (Parent)
		{
			auto It = Context.BoneNodeToIndex.find(Parent);
			if (It != Context.BoneNodeToIndex.end())
			{
				Bone.ParentIndex = It->second;
				break;
			}

			Parent = Parent->GetParent();
		}

		const FbxAMatrix GlobalFbxMatrix = Node->EvaluateGlobalTransform();
		FMatrix GlobalMatrix = FFbxTransformUtils::ToEngineMatrix(GlobalFbxMatrix);
		Bone.LocalMatrix = FFbxTransformUtils::ToEngineMatrix(Node->EvaluateLocalTransform());
		Bone.GlobalMatrix = GlobalMatrix;
		Bone.InverseBindPoseMatrix = FFbxTransformUtils::ToEngineInverseMatrix(GlobalFbxMatrix);

		const int32 NewBoneIndex = static_cast<int32>(Context.Bones.size());
		Context.Bones.push_back(Bone);
		Context.BoneNodeToIndex[Node] = NewBoneIndex;
		return NewBoneIndex;
	}

	static void BuildReferenceSkeleton(FFbxImportContext& Context)
	{
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

		FbxNode* ParentNode = Node->GetParent();
		Bone.ParentIndex = FindNearestParentBoneIndex(Node, Context.BoneNodeToIndex);
		if (Bone.ParentIndex < 0)
		{
			Bone.ParentIndex = AddSyntheticRootBoneIfNeeded(ParentNode, Context);
		}

		const FbxAMatrix GlobalFbxMatrix = Node->EvaluateGlobalTransform();
		Bone.LocalMatrix = FFbxTransformUtils::ToEngineMatrix(Node->EvaluateLocalTransform());
		Bone.GlobalMatrix = FFbxTransformUtils::ToEngineMatrix(GlobalFbxMatrix);
		Bone.InverseBindPoseMatrix = FFbxTransformUtils::ToEngineInverseMatrix(GlobalFbxMatrix);

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
