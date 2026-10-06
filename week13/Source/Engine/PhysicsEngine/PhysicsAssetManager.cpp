#include "PhysicsAssetManager.h"

#include "Asset/AssetPackage.h"
#include "Core/Logging/Log.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Object/Object.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace
{
	std::filesystem::path ResolveProjectPath(const FString& Path)
	{
		std::filesystem::path FullPath(FPaths::ToWide(Path));
		if (!FullPath.is_absolute())
		{
			FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
		}
		return FullPath.lexically_normal();
	}

	bool TryGetSourceFileState(const FString& SourcePath, uint64& OutTimestamp, uint64& OutFileSize)
	{
		const std::filesystem::path FullPath = ResolveProjectPath(SourcePath);
		if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath))
		{
			OutTimestamp = 0;
			OutFileSize = 0;
			return false;
		}

		OutFileSize = static_cast<uint64>(std::filesystem::file_size(FullPath));
		const auto WriteTime = std::filesystem::last_write_time(FullPath);
		OutTimestamp = static_cast<uint64>(WriteTime.time_since_epoch().count());
		return true;
	}

	FAssetImportMetadata MakeImportMetadata(const FString& SourcePath)
	{
		FAssetImportMetadata Metadata;
		Metadata.SourcePath = FPaths::MakeProjectRelative(SourcePath);
		TryGetSourceFileState(SourcePath, Metadata.SourceTimestamp, Metadata.SourceFileSize);
		return Metadata;
	}

	FString GetDisplayNameFromPath(const std::filesystem::path& Path)
	{
		return FPaths::ToUtf8(Path.stem().generic_wstring());
	}
}

UPhysicsAsset* FPhysicsAssetManager::Load(const FString& Path, USkeletalMesh* SourceSkeletalMesh)
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto It = LoadedPhysicsAssets.find(NormalizedPath);
	if (It != LoadedPhysicsAssets.end())
	{
		if (It->second && SourceSkeletalMesh)
		{
			It->second->SetOuter(SourceSkeletalMesh);
		}
		return It->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		return nullptr;
	}

	FWindowsBinReader Reader(NormalizedPath);
	if (!Reader.IsValid())
	{
		UE_LOG("PhysicsAsset load failed: could not open file. Path=%s", NormalizedPath.c_str());
		return nullptr;
	}

	FAssetPackageHeader Header;
	Reader << Header;
	if (!Header.IsValid(EAssetPackageType::PhysicsAsset))
	{
		UE_LOG("PhysicsAsset load failed: invalid package header. Path=%s", NormalizedPath.c_str());
		return nullptr;
	}

	FAssetImportMetadata Metadata;
	Reader << Metadata;

	UPhysicsAsset* PhysicsAsset = UObjectManager::Get().CreateObject<UPhysicsAsset>(SourceSkeletalMesh);
	if (!PhysicsAsset)
	{
		return nullptr;
	}

	PhysicsAsset->SetAssetPathFileName(NormalizedPath);
	PhysicsAsset->Serialize(Reader);

	if (PhysicsAsset->GetSourceSkeletalMeshPath().empty() ||
		PhysicsAsset->GetSourceSkeletalMeshPath() == "None")
	{
		PhysicsAsset->SetSourceSkeletalMeshPath(Metadata.SourcePath.empty() ? FString("None") : Metadata.SourcePath);
	}

	if (!Reader.IsValid())
	{
		UE_LOG("PhysicsAsset load failed: corrupted package. Path=%s", NormalizedPath.c_str());
		UObjectManager::Get().DestroyObject(PhysicsAsset);
		return nullptr;
	}

	LoadedPhysicsAssets[NormalizedPath] = PhysicsAsset;
	return PhysicsAsset;
}

UPhysicsAsset* FPhysicsAssetManager::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto It = LoadedPhysicsAssets.find(NormalizedPath);
	return It != LoadedPhysicsAssets.end() ? It->second : nullptr;
}

bool FPhysicsAssetManager::Save(UPhysicsAsset* PhysicsAsset, const FString& PackagePath, const FString& SourceSkeletalMeshPath)
{
	if (!PhysicsAsset || PackagePath.empty())
	{
		return false;
	}

	const FString NormalizedPath = FPaths::MakeProjectRelative(PackagePath);
	const FString NormalizedSourceMeshPath = SourceSkeletalMeshPath.empty()
		? FString("None")
		: FPaths::MakeProjectRelative(SourceSkeletalMeshPath);

	std::filesystem::path FullPath = ResolveProjectPath(NormalizedPath);
	FPaths::CreateDir(FullPath.parent_path().wstring());

	PhysicsAsset->SetAssetPathFileName(NormalizedPath);
	PhysicsAsset->SetSourceSkeletalMeshPath(NormalizedSourceMeshPath);

	FWindowsBinWriter Writer(NormalizedPath);
	if (!Writer.IsValid())
	{
		UE_LOG("PhysicsAsset save failed: could not open file. Path=%s", NormalizedPath.c_str());
		return false;
	}

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::PhysicsAsset);

	FAssetImportMetadata Metadata = MakeImportMetadata(NormalizedSourceMeshPath);

	Writer << Header;
	Writer << Metadata;
	PhysicsAsset->Serialize(Writer);

	if (!Writer.IsValid())
	{
		UE_LOG("PhysicsAsset save failed: write failed. Path=%s", NormalizedPath.c_str());
		return false;
	}

	LoadedPhysicsAssets[NormalizedPath] = PhysicsAsset;

	auto ListIt = std::find_if(
		AvailablePhysicsAssetFiles.begin(),
		AvailablePhysicsAssetFiles.end(),
		[&](const FAssetListItem& Item)
		{
			return Item.FullPath == NormalizedPath;
		});

	if (ListIt == AvailablePhysicsAssetFiles.end())
	{
		FAssetListItem Item;
		Item.DisplayName = GetDisplayNameFromPath(FullPath);
		Item.FullPath = NormalizedPath;
		AvailablePhysicsAssetFiles.push_back(Item);
	}

	return true;
}

bool FPhysicsAssetManager::SaveForSkeletalMesh(USkeletalMesh* SkeletalMesh, const FString& SkeletalMeshPackagePath)
{
	if (!SkeletalMesh || !SkeletalMesh->GetPhysicsAsset())
	{
		return false;
	}

	const FString MeshPackagePath = !SkeletalMeshPackagePath.empty()
		? FPaths::MakeProjectRelative(SkeletalMeshPackagePath)
		: SkeletalMesh->GetAssetPathFileName();

	if (MeshPackagePath.empty() || MeshPackagePath == "None")
	{
		return false;
	}

	UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	PhysicsAsset->SetOuter(SkeletalMesh);

	const FString PhysicsAssetPath = GetPhysicsAssetPackagePath(MeshPackagePath);
	const bool bSaved = Save(PhysicsAsset, PhysicsAssetPath, MeshPackagePath);
	if (bSaved)
	{
		SkeletalMesh->SetPhysicsAssetPath(PhysicsAssetPath);
	}
	return bSaved;
}

void FPhysicsAssetManager::RefreshAvailablePhysicsAssets()
{
	AvailablePhysicsAssetFiles.clear();

	namespace fs = std::filesystem;
	const fs::path ContentDir = fs::path(FPaths::RootDir()) / L"Content";
	if (!fs::exists(ContentDir) || !fs::is_directory(ContentDir))
	{
		return;
	}

	const fs::path ProjectRoot(FPaths::RootDir());
	for (const auto& Entry : fs::recursive_directory_iterator(ContentDir))
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

		const FString RelPath = FPaths::ToUtf8(Entry.path().lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (!FAssetPackage::ReadMetadata(RelPath, EAssetPackageType::PhysicsAsset, Metadata))
		{
			continue;
		}

		FAssetListItem Item;
		Item.DisplayName = GetDisplayNameFromPath(Entry.path());
		Item.FullPath = RelPath;
		AvailablePhysicsAssetFiles.push_back(Item);
	}
}

FString FPhysicsAssetManager::GetPhysicsAssetPackagePath(const FString& SkeletalMeshPath)
{
	std::filesystem::path ProjectRelative =
		std::filesystem::path(FPaths::ToWide(FPaths::MakeProjectRelative(SkeletalMeshPath))).lexically_normal();

	std::filesystem::path AssetPath = (!ProjectRelative.empty() && ProjectRelative.begin()->wstring() == L"Content")
		? ProjectRelative
		: (std::filesystem::path(L"Content") / ProjectRelative);

	std::wstring Stem = AssetPath.stem().wstring();
	const std::wstring SkeletalSuffix = L"_SkeletalMesh";
	if (Stem.size() >= SkeletalSuffix.size() &&
		Stem.compare(Stem.size() - SkeletalSuffix.size(), SkeletalSuffix.size(), SkeletalSuffix) == 0)
	{
		Stem.erase(Stem.size() - SkeletalSuffix.size());
	}

	AssetPath.replace_filename(Stem + L"_PhysicsAsset.uasset");

	const std::filesystem::path FullAssetPath = std::filesystem::path(FPaths::RootDir()) / AssetPath;
	FPaths::CreateDir(FullAssetPath.parent_path().wstring());

	return FPaths::ToUtf8(AssetPath.generic_wstring());
}

void FPhysicsAssetManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : LoadedPhysicsAssets)
	{
		Collector.AddReferencedObject(Pair.second, "FPhysicsAssetManager.LoadedPhysicsAssets");
	}
}

void FPhysicsAssetManager::ClearCache()
{
	LoadedPhysicsAssets.clear();
	AvailablePhysicsAssetFiles.clear();
}
