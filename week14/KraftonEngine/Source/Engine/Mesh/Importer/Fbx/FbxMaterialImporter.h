#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"

#include <fbxsdk.h>

class FFbxMaterialImporter
{
public:
	static void CollectMaterials(FbxScene* Scene, FFbxImportContext& Context);
	static int32 GetMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex);
	static void BuildStaticMaterials(const FFbxImportContext& Context, TArray<FStaticMaterial>& OutMaterials);
	static void BuildSkeletalMaterials(const FFbxImportContext& Context, const TArray<FSkeletalMeshSection>& Sections, TArray<FSkeletalMaterial>& OutMaterials, TArray<FSkeletalMeshSection>& InOutSections);
	static FString CreateOrUpdateMaterialAsset(const FFbxImportedMaterialInfo& MaterialInfo);
};
