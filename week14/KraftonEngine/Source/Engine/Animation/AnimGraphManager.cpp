#include "AnimGraphManager.h"

#include "AnimGraphAsset.h"
#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

UAnimGraphAsset* FAnimGraphManager::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedGraphs.find(NormalizedPath);
	if (It != LoadedGraphs.end())
	{
		return It->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath)) return nullptr;

	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid()) return nullptr;

	FAssetPackageHeader Header;
	Ar << Header;
	if (!Header.IsValid(EAssetPackageType::AnimGraph)) return nullptr;

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UAnimGraphAsset* NewAsset = UObjectManager::Get().CreateObject<UAnimGraphAsset>();
	NewAsset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}

	NewAsset->SetSourcePath(NormalizedPath);
	LoadedGraphs.emplace(NormalizedPath, NewAsset);
	return NewAsset;
}

UAnimGraphAsset* FAnimGraphManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto It = LoadedGraphs.find(NormalizedPath);
	return It != LoadedGraphs.end() ? It->second : nullptr;
}

bool FAnimGraphManager::Save(UAnimGraphAsset* Asset)
{
	if (!Asset) return false;

	const FString& Path = Asset->GetSourcePath();
	if (Path.empty()) return false;

	FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::AnimGraph);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;

	Asset->Serialize(Ar);
	return Ar.IsValid();
}

void FAnimGraphManager::RefreshAvailableGraphs()
{
	const std::filesystem::path ContentRoot = std::filesystem::path(FPaths::RootDir()) / L"Content";
	if (!std::filesystem::exists(ContentRoot)) return;

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	AvailableGraphFiles.clear();

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
	{
		if (!Entry.is_regular_file()) continue;

		std::wstring Ext = Entry.path().extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".uasset") continue;

		const FString RelPath =
			FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::AnimGraph, Metadata))
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Entry.path().stem().wstring());
		Item.FullPath    = RelPath;
		AvailableGraphFiles.push_back(std::move(Item));
	}
}
