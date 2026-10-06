#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Object/GarbageCollection.h"

class UPhysicsAsset;
class USkeletalMesh;

class FPhysicsAssetManager : public TSingleton<FPhysicsAssetManager>, public FGCObject
{
	friend class TSingleton<FPhysicsAssetManager>;

public:
	UPhysicsAsset* Load(const FString& Path, USkeletalMesh* SourceSkeletalMesh = nullptr);
	UPhysicsAsset* Find(const FString& Path) const;

	bool Save(UPhysicsAsset* PhysicsAsset, const FString& PackagePath, const FString& SourceSkeletalMeshPath);
	bool SaveForSkeletalMesh(USkeletalMesh* SkeletalMesh, const FString& SkeletalMeshPackagePath = FString());

	void RefreshAvailablePhysicsAssets();
	const TArray<FAssetListItem>& GetAvailablePhysicsAssetFiles() const { return AvailablePhysicsAssetFiles; }

	static FString GetPhysicsAssetPackagePath(const FString& SkeletalMeshPath);

	const char* GetReferencerName() const override { return "FPhysicsAssetManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	void ClearCache();

private:
	TMap<FString, UPhysicsAsset*> LoadedPhysicsAssets;
	TArray<FAssetListItem> AvailablePhysicsAssetFiles;
};
