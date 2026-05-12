#pragma once

#include "Asset/IAssetLoader.h"
#include "Asset/SkeletalMeshTypes.h"
#include "Asset/StaticMeshTypes.h"
#include "Core/ResourceTypes.h"

namespace fbxsdk
{
	class FbxManager;
	class FbxScene;
	class FbxNode;
    class FbxMesh;
    class FbxAMatrix;
}

enum class ESkeletalMeshImportPass
{
    SkinnedMeshes,
    RigidAttachedMeshes
};

struct FFbxMeshContentInfo
{
    bool bHasStaticMesh = false;
    bool bHasSkeletalMesh = false;
};

class FFbxImporter : public IAssetLoader
{
public:
	FFbxImporter() = default;
	~FFbxImporter() override = default;

	FStaticMesh* Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions);

	bool SupportsExtension(const FString& Extension) const override;
	FString GetLoaderName() const override;

	/*
	 * note: 병합하면서 충돌이 발생하거나 동일한 로직의 함수가 있다면 날려버리셔도 됩니다
     */
	FSkeletalMesh* LoadSkeletalMesh(const FString& Path, const FStaticMeshLoadOptions& LoadOptions);

	FFbxMeshContentInfo InspectMeshContent(const FString& Path);

private:
	bool ImportScene(const FString& Path, fbxsdk::FbxManager* Manager, fbxsdk::FbxScene* Scene);

	// Scene -> StaticMesh (mesh node를 재귀로 순회)
	void CollectMeshes(fbxsdk::FbxNode* Node, FStaticMesh* InStaticMesh);
	void ProcessMesh(fbxsdk::FbxMesh* Mesh, FStaticMesh* InStaticMesh);

	int32 GetOrAddMaterialSlot(FStaticMesh* InStaticMesh, const FString& MaterialName);
	FAABB BuildLocalBounds(FStaticMesh* InStaticMesh) const;

	void NormalizePositionsToUnitCube(FStaticMesh* InStaticMesh);
	void ComputeTangents(FStaticMesh* InStaticMesh);

	/*
     * note: 병합하면서 충돌이 발생하거나 동일한 로직의 함수가 있다면 날려버리셔도 됩니다
	 *       아래 함수들 모두 해당!
     */
    void CollectSkeletalMeshes(
        fbxsdk::FbxNode* Node,
        FSkeletalMesh* InSkeletalMesh,
        ESkeletalMeshImportPass Pass,
        TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex,
        bool& bHasImportedSkinnedMesh);

    void ProcessSkeletalMesh(
        fbxsdk::FbxMesh* Mesh,
        FSkeletalMesh* InSkeletalMesh,
        ESkeletalMeshImportPass Pass,
        TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex,
        bool& bHasImportedSkinnedMesh);

    void ProcessRigidAttachedMesh(
        fbxsdk::FbxMesh* Mesh,
        FSkeletalMesh* InSkeletalMesh,
        TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex,
        bool bHasImportedSkinnedMesh);

    int32 GetOrAddMaterialSlot(FSkeletalMesh* InSkeletalMesh, const FString& MaterialName);
    FAABB BuildLocalBounds(FSkeletalMesh* InSkeletalMesh) const;
    void ComputeTangents(FSkeletalMesh* InSkeletalMesh);
};
