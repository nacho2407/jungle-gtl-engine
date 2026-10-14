#pragma once

#include "Asset/AssetRegistry.h"
#include "Core/Types/CoreTypes.h"

class FReferenceCollector;
class UPhysicalMaterial;

class FPhysicalMaterialManager
{
public:
	static FPhysicalMaterialManager& Get();

	UPhysicalMaterial* Load(const FString& Path);
	UPhysicalMaterial* Find(const FString& Path) const;
	bool Save(UPhysicalMaterial* Material);

	void RefreshAvailablePhysicalMaterials();
	const TArray<FAssetListItem>& GetAvailablePhysicalMaterialFiles() const { return AvailablePhysicalMaterialFiles; }

	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	FPhysicalMaterialManager() = default;

	TMap<FString, UPhysicalMaterial*> LoadedPhysicalMaterials;
	TArray<FAssetListItem> AvailablePhysicalMaterialFiles;
};
