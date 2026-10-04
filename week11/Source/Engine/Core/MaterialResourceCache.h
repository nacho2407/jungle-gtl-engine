#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/Material.h"

class FMaterialResourceCache
{
public:
	UMaterial* GetMaterial(const FString& MaterialName) const;
	UMaterial* FindMaterialByKey(const FString& Key) const;
	void RegisterMaterial(const FString& Key, UMaterial* Material);
	bool ContainsMaterialKey(const FString& Key) const;
	TArray<FString> GetMaterialNames() const;
	TArray<FString> GetMaterialInterfaceNames(const TArray<FString>& MaterialFilePaths) const;

	UMaterialInstance* CreateMaterialInstance(const FString& Path, UMaterial* Parent);
	UMaterialInstance* GetMaterialInstance(const FString& Path) const;
	void RegisterMaterialInstance(const FString& Path, UMaterialInstance* Instance);

	const FString* FindMaterialSlotAlias(const FString& Key) const;
	void SetMaterialSlotAlias(const FString& Key, const FString& Value);

	size_t GetMaterialMemorySize() const;
	void Release();

private:
	TMap<FString, UMaterial*> Materials;
	TMap<FString, UMaterialInstance*> MaterialInstances;
	TMap<FString, FString> MaterialSlotAliases;
};
