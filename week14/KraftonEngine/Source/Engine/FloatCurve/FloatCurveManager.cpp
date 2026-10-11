#include "FloatCurveManager.h"
#include "FloatCurveAsset.h"
#include "Platform/Paths.h"
#include "Asset/AssetPackage.h"
#include "Serialization/WindowsArchive.h"

UFloatCurveAsset* FFloatCurveManager::Load(const FString& Path)
{
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto it = LoadedCurves.find(NormalizedPath);
	if (it != LoadedCurves.end())
	{
		return it->second;
	}

	if (!FAssetPackage::IsAssetPackagePath(NormalizedPath)) return nullptr;

	FWindowsBinReader Ar(NormalizedPath);
	if (!Ar.IsValid()) return nullptr;

	FAssetPackageHeader Header;
	Ar << Header;
	if (!Header.IsValid(EAssetPackageType::FloatCurve)) return nullptr;

	FAssetImportMetadata Metadata;
	Ar << Metadata;

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	NewAsset->Serialize(Ar);

	if (!Ar.IsValid())
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}

	NewAsset->SetSourcePath(NormalizedPath);
	LoadedCurves.emplace(NormalizedPath, NewAsset);
	return NewAsset;
}

UFloatCurveAsset* FFloatCurveManager::Find(const FString& Path) const
{
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto it = LoadedCurves.find(NormalizedPath);
	return it != LoadedCurves.end() ? it->second : nullptr;
}

bool FFloatCurveManager::Save(UFloatCurveAsset* Asset)
{
	if (!Asset)
	{
		return false;
	}

	const FString& Path = Asset->GetSourcePath();
	if (Path.empty()) return false;

	FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(EAssetPackageType::FloatCurve);

	FAssetImportMetadata Metadata;

	Ar << Header;
	Ar << Metadata;

	Asset->Serialize(Ar);

	return Ar.IsValid();
}
