#include "Physics/PhysicalMaterialManager.h"

#include "Asset/AssetPackage.h"
#include "Object/ReferenceCollector.h"
#include "Physics/PhysicalMaterial.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

FPhysicalMaterialManager& FPhysicalMaterialManager::Get()
{
	static FPhysicalMaterialManager Instance;
	return Instance;
}

UPhysicalMaterial* FPhysicalMaterialManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedPhysicalMaterials.find(NormalizedPath);
	if (It != LoadedPhysicalMaterials.end())
	{
		return It->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		return nullptr;
	}

	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid())
	{
		return nullptr;
	}

	FAssetPackageHeader Header;
	Ar << Header;
	if (!Header.IsValid(EAssetPackageType::PhysicalMaterial))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UPhysicalMaterial* NewMaterial = UObjectManager::Get().CreateObject<UPhysicalMaterial>();
	NewMaterial->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UObjectManager::Get().DestroyObject(NewMaterial);
		return nullptr;
	}

	NewMaterial->SetAssetPathFileName(NormalizedPath);
	LoadedPhysicalMaterials.emplace(NormalizedPath, NewMaterial);
	return NewMaterial;
}

UPhysicalMaterial* FPhysicalMaterialManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedPhysicalMaterials.find(NormalizedPath);
	return It != LoadedPhysicalMaterials.end() ? It->second : nullptr;
}

bool FPhysicalMaterialManager::Save(UPhysicalMaterial* Material)
{
	if (!Material)
	{
		return false;
	}

	const FString& Path = Material->GetAssetPathFileName();
	if (Path.empty() || Path == "None")
	{
		return false;
	}

	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	FWindowsBinWriter Ar(NormalizedPath);
	if (!Ar.IsValid())
	{
		return false;
	}

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::PhysicalMaterial);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;
	Material->Serialize(Ar);

	if (!Ar.IsValid())
	{
		return false;
	}

	LoadedPhysicalMaterials[NormalizedPath] = Material;
	RefreshAvailablePhysicalMaterials();
	return true;
}

void FPhysicalMaterialManager::RefreshAvailablePhysicalMaterials()
{
	namespace fs = std::filesystem;

	AvailablePhysicalMaterialFiles.clear();

	const fs::path ContentRoot = fs::path(FPaths::RootDir()) / L"Content";
	if (!fs::exists(ContentRoot) || !fs::is_directory(ContentRoot))
	{
		return;
	}

	const fs::path ProjectRoot(FPaths::RootDir());
	for (const auto& Entry : fs::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		std::wstring Ext = Entry.path().extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".uasset")
		{
			continue;
		}

		const FString RelPath =
			FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::PhysicalMaterial, Metadata))
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().generic_wstring());
		Item.FullPath = RelPath;
		AvailablePhysicalMaterialFiles.push_back(std::move(Item));
	}
}

void FPhysicalMaterialManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Path, Material] : LoadedPhysicalMaterials)
	{
		Collector.AddReferencedObject(Material);
	}
}
