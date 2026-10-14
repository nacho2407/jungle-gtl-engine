#include "CameraShakeManager.h"
#include "CameraShakeAsset.h"
#include "Asset/AssetPackage.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

UCameraShakeAsset* FCameraShakeManager::Load(const FString& Path)
{
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto it = LoadedShakes.find(NormalizedPath);
	if (it != LoadedShakes.end())
	{
		return it->second;
	}

	if (FAssetPackage::IsAssetPackagePath(NormalizedPath))
	{
		FWindowsBinReader Ar(NormalizedPath);
		if (!Ar.IsValid()) return nullptr;

		FAssetPackageHeader Header;
		Ar << Header;
		if (!Header.IsValid(EAssetPackageType::CameraShake)) return nullptr;

		FAssetImportMetadata Metadata;
		Ar << Metadata;

		UCameraShakeAsset* NewAsset = UObjectManager::Get().CreateObject<UCameraShakeAsset>();
		NewAsset->Serialize(Ar);

		if (!Ar.IsValid())
		{
			UObjectManager::Get().DestroyObject(NewAsset);
			return nullptr;
		}

		NewAsset->SetSourcePath(NormalizedPath);
		LoadedShakes.emplace(NormalizedPath, NewAsset);
		return NewAsset;
	}

	UCameraShakeAsset* NewAsset = UObjectManager::Get().CreateObject<UCameraShakeAsset>();
	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(NormalizedPath)));
	if (NewAsset->LoadFromFile(FullPath))
	{
		NewAsset->SetSourcePath(NormalizedPath);
		LoadedShakes.emplace(NormalizedPath, NewAsset);
		return NewAsset;
	}
	else
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}
}

UCameraShakeAsset* FCameraShakeManager::Find(const FString& Path) const
{
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);
	auto it = LoadedShakes.find(NormalizedPath);
	return it != LoadedShakes.end() ? it->second : nullptr;
}

bool FCameraShakeManager::Save(UCameraShakeAsset* Asset)
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

	if (!FAssetPackage::IsAssetPackagePath(Path))
	{
		const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Path)));
		return Asset->SaveToFile(FullPath);
	}

	FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::CameraShake);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;

	Asset->Serialize(Ar);

	return Ar.IsValid();
}
