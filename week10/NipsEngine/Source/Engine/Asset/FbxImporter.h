#pragma once

#include "Asset/StaticMeshTypes.h"
#include "Asset/IAssetLoader.h"
#include <Core/ResourceTypes.h>

namespace fbxsdk
{
	class FbxManager;
	class FbxScene;
	class FbxNode;
	class FbxMesh;
}

class FFbxImporter : public IAssetLoader
{
public:
	FFbxImporter() = default;
	~FFbxImporter() override = default;

	FStaticMesh* Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions);

	bool SupportsExtension(const FString& Extension) const override;
	FString GetLoaderName() const override;

private:
	bool ImportScene(const FString& Path, fbxsdk::FbxManager* Manager, fbxsdk::FbxScene* Scene);

	// Scene -> StaticMesh (mesh node를 재귀로 순회)
	void CollectMeshes(fbxsdk::FbxNode* Node, FStaticMesh* InStaticMesh);
	void ProcessMesh(fbxsdk::FbxMesh* Mesh, FStaticMesh* InStaticMesh);

	int32 GetOrAddMaterialSlot(FStaticMesh* InStaticMesh, const FString& MaterialName);
	FAABB BuildLocalBounds(FStaticMesh* InStaticMesh) const;

	void NormalizePositionsToUnitCube(FStaticMesh* InStaticMesh);
	void ComputeTangents(FStaticMesh* InStaticMesh);
};
