#include "Core/MaterialResourceCache.h"

#include "Core/Paths.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <unordered_set>

namespace
{
	bool IsSerializedMaterialAssetPathForCache(const FString& Path)
	{
		std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
		std::wstring Extension = FsPath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension == L".mat" || Extension == L".matinst";
	}
}

UMaterial* FMaterialResourceCache::GetMaterial(const FString& MaterialName) const
{
	const FString NormalizedName = FPaths::Normalize(MaterialName);
	auto It = Materials.find(MaterialName);
	if (It != Materials.end())
	{
		return It->second;
	}
	It = Materials.find(NormalizedName);
	if (It != Materials.end())
	{
		return It->second;
	}
	for (const auto& [Name, Material] : Materials)
	{
		if (Material && (Material->GetName() == MaterialName || FPaths::Normalize(Material->GetFilePath()) == NormalizedName))
		{
			return Material;
		}
	}
	return nullptr;
}

UMaterial* FMaterialResourceCache::FindMaterialByKey(const FString& Key) const
{
	auto It = Materials.find(Key);
	return (It != Materials.end()) ? It->second : nullptr;
}

void FMaterialResourceCache::RegisterMaterial(const FString& Key, UMaterial* Material)
{
	if (Key.empty() || Material == nullptr)
	{
		return;
	}

	Materials[Key] = Material;
}

bool FMaterialResourceCache::ContainsMaterialKey(const FString& Key) const
{
	return Materials.find(Key) != Materials.end();
}

TArray<FString> FMaterialResourceCache::GetMaterialNames() const
{
	TArray<FString> Names;
	Names.reserve(Materials.size());
	for (const auto& [Name, Mat] : Materials)
	{
		Names.push_back(Name);
	}
	return Names;
}

TArray<FString> FMaterialResourceCache::GetMaterialInterfaceNames(const TArray<FString>& MaterialFilePaths) const
{
	TArray<FString> Names;
	Names.reserve(Materials.size() + MaterialInstances.size() + MaterialFilePaths.size());
	std::unordered_set<FString> SeenNames;
	std::unordered_set<const UMaterial*> SeenMaterials;
	for (const auto& [Name, Mat] : Materials)
	{
		if (Mat && SeenMaterials.insert(Mat).second)
		{
			const std::filesystem::path FilePath(FPaths::ToWide(Mat->GetFilePath()));
			const bool bFileBackedMaterial = FilePath.extension() == L".mat";
			const FString DisplayName = bFileBackedMaterial ? FPaths::Normalize(Mat->GetFilePath()) : Name;
			if (SeenNames.insert(DisplayName).second)
			{
				Names.push_back(DisplayName);
			}
		}
	}
	for (const auto& [Path, MatInst] : MaterialInstances)
	{
		if (MatInst)
		{
			const FString DisplayName = FPaths::Normalize(MatInst->GetFilePath().empty() ? Path : MatInst->GetFilePath());
			if (SeenNames.insert(DisplayName).second)
			{
				Names.push_back(DisplayName);
			}
		}
	}
	for (const FString& Path : MaterialFilePaths)
	{
		const FString NormalizedPath = FPaths::Normalize(Path);
		if (IsSerializedMaterialAssetPathForCache(NormalizedPath) && SeenNames.insert(NormalizedPath).second)
		{
			Names.push_back(NormalizedPath);
		}
	}
	return Names;
}

UMaterialInstance* FMaterialResourceCache::CreateMaterialInstance(const FString& Path, UMaterial* Parent)
{
	FString NormalizedPath = FPaths::Normalize(Path);

	if (UMaterialInstance* Existing = GetMaterialInstance(NormalizedPath))
	{
		if (Existing->Parent == nullptr)
		{
			Existing->Parent = Parent;
		}
		return Existing;
	}

	UMaterialInstance* Instance = UObjectManager::Get().CreateObject<UMaterialInstance>();
	Instance->Parent = Parent;
	Instance->Name = NormalizedPath;
	Instance->FilePath = NormalizedPath;
	MaterialInstances[NormalizedPath] = Instance;
	return Instance;
}

UMaterialInstance* FMaterialResourceCache::GetMaterialInstance(const FString& Path) const
{
	auto It = MaterialInstances.find(FPaths::Normalize(Path));
	return (It != MaterialInstances.end()) ? It->second : nullptr;
}

void FMaterialResourceCache::RegisterMaterialInstance(const FString& Path, UMaterialInstance* Instance)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || Instance == nullptr)
	{
		return;
	}

	MaterialInstances[NormalizedPath] = Instance;
}

const FString* FMaterialResourceCache::FindMaterialSlotAlias(const FString& Key) const
{
	auto It = MaterialSlotAliases.find(Key);
	return (It != MaterialSlotAliases.end()) ? &It->second : nullptr;
}

void FMaterialResourceCache::SetMaterialSlotAlias(const FString& Key, const FString& Value)
{
	if (Key.empty())
	{
		return;
	}

	MaterialSlotAliases[Key] = Value;
}

size_t FMaterialResourceCache::GetMaterialMemorySize() const
{
	size_t TotalSize = 0;

	TotalSize += Materials.size() * sizeof(UMaterial);

	for (const auto& Pair : Materials)
	{
		const FMaterial& Mat = Pair.second->MaterialData;
		TotalSize += Mat.Name.capacity();
		TotalSize += Mat.DiffuseTexPath.capacity();
		TotalSize += Mat.AmbientTexPath.capacity();
		TotalSize += Mat.SpecularTexPath.capacity();
		TotalSize += Mat.BumpTexPath.capacity();
	}

	return TotalSize;
}

void FMaterialResourceCache::Release()
{
	std::unordered_set<UObject*> DestroyedObjects;
	auto DestroyUniqueObject = [&DestroyedObjects](UObject* Object)
	{
		if (Object && DestroyedObjects.insert(Object).second)
		{
			UObjectManager::Get().DestroyObject(Object);
		}
	};

	for (auto& [Key, Material] : Materials)
	{
		DestroyUniqueObject(Material);
	}
	Materials.clear();

	for (auto& [Key, MaterialInst] : MaterialInstances)
	{
		DestroyUniqueObject(MaterialInst);
	}
	MaterialInstances.clear();

	MaterialSlotAliases.clear();
}
