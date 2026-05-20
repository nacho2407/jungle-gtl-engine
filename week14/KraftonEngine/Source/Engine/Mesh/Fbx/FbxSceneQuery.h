#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>

class FFbxSceneQuery
{
public:
	static void CollectAllNodes(FbxNode* RootNode, TArray<FbxNode*>& OutNodes);
	static void CollectMeshNodes(FbxNode* RootNode, TArray<FbxNode*>& OutMeshNodes);
	static bool IsSkeletonNode(FbxNode* Node);
	static bool HasNonSkeletonWrapperParent(FbxNode* Node);
	static bool MeshHasSkin(FbxMesh* Mesh);
	static bool SceneHasSkinDeformer(FbxScene* Scene);
	static bool IsValidControlPointIndex(const FbxMesh* Mesh, int32 ControlPointIndex);
};
