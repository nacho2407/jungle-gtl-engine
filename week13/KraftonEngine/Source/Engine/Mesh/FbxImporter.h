#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Render/Types/VertexTypes.h"
#include "SkeletalMeshAsset.h"

#include <fbxsdk.h>

class FFbxImporter
{
public:
	static bool Import(const FString& FilePath);

private:
	static bool Parse(FbxNode* RootNode);

	static void CollectNodes(FbxNode* Node, int32 depth, TArray<FbxNode*>& OutNodes);
	static void ParseBone(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& OutNodeToIndex);
	static void ParseSkin(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& NodeToIndex);

public:
	// Temporary data structures for parsing
	static TArray<FVertexPNCTBW> Vertices;
	static TArray<uint32> Indices;
	static TArray<FBone> Bones;
};
