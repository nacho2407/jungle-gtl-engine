#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include "Object/GarbageCollection.h"

class ULuaBlueprintAsset;

class FLuaBlueprintManager : public TSingleton<FLuaBlueprintManager>, public FGCObject
{
	friend class TSingleton<FLuaBlueprintManager>;

public:
	ULuaBlueprintAsset* Load(const FString& Path);
	ULuaBlueprintAsset* Find(const FString& Path) const;
	bool Save(ULuaBlueprintAsset* Asset);

	void RefreshAvailableBlueprints();
	const TArray<FAssetListItem>& GetAvailableBlueprintFiles() const { return AvailableBlueprintFiles; }

	const char* GetReferencerName() const override { return "FLuaBlueprintManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	void ClearCache();

private:
	TMap<FString, ULuaBlueprintAsset*> LoadedBlueprints;
	TArray<FAssetListItem>             AvailableBlueprintFiles;
};
