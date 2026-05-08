#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Render/Types/VertexTypes.h"
#include "SkeletalMeshAsset.h"

#include <fbxsdk.h>

class FFbxImporter
{
	struct FMaterialInfo
	{
		FString Name;
		FVector DiffuseColor;
		FString TexturePath;
	};

public:
	static bool Import(const FString& FilePath);

private:
	static bool Parse(FbxScene* Scene);
	static bool Convert();

	static void CollectNodes(FbxNode* Node, int32 depth, TArray<FbxNode*>& OutNodes);
	static void CollectMaterials(FbxScene* Scene);

	static void ParseBone(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& OutNodeToIndex);
	static void ParseSkin(TArray<FbxNode*>& Nodes, TMap<FbxNode*, int32>& NodeToIndex);

	static int32 GetMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex);
	static void TriangulateScene(FbxScene* Scene);

	static FString ConvertToMat(const FMaterialInfo* MaterialInfo);

public:
	// Temporary data structures for parsing
	static TArray<FVertexPNCTBW> Vertices;
	static TArray<uint32> Indices;
	static TArray<FBone> Bones;
	static TArray<FSkeletalMeshSection> Sections;
	static TArray<FMaterialInfo> MtlInfos;
	static TMap<FbxSurfaceMaterial*, int32> MaterialToSlotIndex;
	static TArray<FSkeletalMaterial> SkeletalMaterials;

};
