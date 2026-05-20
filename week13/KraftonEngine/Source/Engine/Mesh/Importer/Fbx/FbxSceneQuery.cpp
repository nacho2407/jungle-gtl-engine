#include "Mesh/Importer/Fbx/FbxSceneQuery.h"

void FFbxSceneQuery::CollectAllNodes(FbxNode* RootNode, TArray<FbxNode*>& OutNodes)
{
	if (!RootNode)
	{
		return;
	}

	OutNodes.push_back(RootNode);
	for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
	{
		CollectAllNodes(RootNode->GetChild(ChildIndex), OutNodes);
	}
}

void FFbxSceneQuery::CollectMeshNodes(FbxNode* RootNode, TArray<FbxNode*>& OutMeshNodes)
{
	if (!RootNode)
	{
		return;
	}

	if (RootNode->GetMesh())
	{
		OutMeshNodes.push_back(RootNode);
	}

	for (int32 ChildIndex = 0; ChildIndex < RootNode->GetChildCount(); ++ChildIndex)
	{
		CollectMeshNodes(RootNode->GetChild(ChildIndex), OutMeshNodes);
	}
}

bool FFbxSceneQuery::IsSkeletonNode(FbxNode* Node)
{
	FbxNodeAttribute* Attr = Node ? Node->GetNodeAttribute() : nullptr;
	return Attr && Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton;
}

bool FFbxSceneQuery::HasNonSkeletonWrapperParent(FbxNode* Node)
{
	for (FbxNode* Parent = Node ? Node->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
	{
		if (IsSkeletonNode(Parent))
		{
			return false;
		}

		if (Parent->GetParent())
		{
			return true;
		}
	}

	return false;
}

bool FFbxSceneQuery::MeshHasSkin(FbxMesh* Mesh)
{
	if (!Mesh)
	{
		return false;
	}

	const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
	for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
	{
		FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
		if (Skin && Skin->GetClusterCount() > 0)
		{
			return true;
		}
	}
	return false;
}

bool FFbxSceneQuery::SceneHasSkinDeformer(FbxScene* Scene)
{
	if (!Scene || !Scene->GetRootNode())
	{
		return false;
	}

	TArray<FbxNode*> MeshNodes;
	CollectMeshNodes(Scene->GetRootNode(), MeshNodes);
	for (FbxNode* Node : MeshNodes)
	{
		if (MeshHasSkin(Node ? Node->GetMesh() : nullptr))
		{
			return true;
		}
	}
	return false;
}

bool FFbxSceneQuery::IsValidControlPointIndex(const FbxMesh* Mesh, int32 ControlPointIndex)
{
	return Mesh && ControlPointIndex >= 0 && ControlPointIndex < Mesh->GetControlPointsCount();
}
