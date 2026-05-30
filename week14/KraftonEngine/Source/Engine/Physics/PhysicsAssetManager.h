#pragma once

#include "Core/Types/CoreTypes.h"
#include "Asset/AssetRegistry.h" 

class FReferenceCollector;
class UPhysicsAsset;

class FPhysicsAssetManager
{
public:
	static FPhysicsAssetManager& Get();
	
	UPhysicsAsset* Load(const FString& Path);
	UPhysicsAsset* Find(const FString& Path) const;
	bool Save(UPhysicsAsset* Asset);
	
	const TArray<FAssetListItem>& GetAvailableFiles();
	void ScanAssets();
	
	void AddReferencedObjects(FReferenceCollector& Collector);
	
private:
	FPhysicsAssetManager() = default;
	
	TMap<FString, UPhysicsAsset*> LoadedAssets;
	TArray<FAssetListItem> AvailableFiles;
};
