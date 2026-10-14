#include "PhysicsAssetManager.h"

#include <filesystem>

#include "Physics/PhysicsAsset.h"
#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "Object/ReferenceCollector.h"

FPhysicsAssetManager& FPhysicsAssetManager::Get()
{
	static FPhysicsAssetManager Instance;
	return Instance;
}

UPhysicsAsset* FPhysicsAssetManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedAssets.find(NormalizedPath);
	if (It != LoadedAssets.end())
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
	if (!Header.IsValid(EAssetPackageType::PhysicsAsset))
	{
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UPhysicsAsset* NewAsset = UObjectManager::Get().CreateObject<UPhysicsAsset>();
	NewAsset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}

	NewAsset->SetSourcePath(NormalizedPath);
	LoadedAssets.emplace(NormalizedPath, NewAsset);
	return NewAsset;
}

UPhysicsAsset* FPhysicsAssetManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedAssets.find(NormalizedPath);
	return It != LoadedAssets.end() ? It->second : nullptr;
}

bool FPhysicsAssetManager::Save(UPhysicsAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	const FString& Path = Asset->GetSourcePath();
	if (Path.empty())
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
	Header.Type = static_cast<uint32>(EAssetPackageType::PhysicsAsset);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;
	Asset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		return false;
	}

	LoadedAssets[NormalizedPath] = Asset;

	auto ListIt = std::find_if(
		AvailableFiles.begin(), AvailableFiles.end(),
		[&](const FAssetListItem& Item) { return Item.FullPath == NormalizedPath; });

	if (ListIt == AvailableFiles.end())
	{
		std::filesystem::path P(FPaths::ToWide(NormalizedPath));
		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(P.stem().generic_wstring());
		Item.FullPath    = NormalizedPath;
		AvailableFiles.push_back(Item);
	}

	return true;
}

void FPhysicsAssetManager::ScanAssets()
{
	AvailableFiles.clear();

	namespace fs = std::filesystem;
	const fs::path ContentDir = fs::path(FPaths::RootDir()) / L"Content";
	if (!fs::exists(ContentDir) || !fs::is_directory(ContentDir))
	{
		return;
	}

	for (const auto& Entry : fs::recursive_directory_iterator(ContentDir))
	{
		if (!Entry.is_regular_file() || Entry.path().extension() != L".uasset")
		{
			continue;
		}

		const FString RelPath = FPaths::MakeProjectRelative(FPaths::ToUtf8(Entry.path().generic_wstring()));

		EAssetPackageType Type = EAssetPackageType::Unknown;
		if (!FAssetPackage::GetPackageType(RelPath, Type) || Type != EAssetPackageType::PhysicsAsset)
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().generic_wstring());
		Item.FullPath    = RelPath;
		AvailableFiles.push_back(Item);
	}
}

const TArray<FAssetListItem>& FPhysicsAssetManager::GetAvailableFiles()
{
	if (AvailableFiles.empty())
	{
		ScanAssets();
	}
	return AvailableFiles;
}

void FPhysicsAssetManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& [Path, Asset] : LoadedAssets)
	{
		Collector.AddReferencedObject(Asset);
	}
}