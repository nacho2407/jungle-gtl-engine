#include "FloatCurveManager.h"
#include "FloatCurveAsset.h"
#include "Platform/Paths.h"

UFloatCurveAsset* FFloatCurveManager::Load(const FString& Path)
{
	auto it = LoadedCurves.find(Path);
	if (it != LoadedCurves.end())
	{
		return it->second;
	}

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), FPaths::ToWide(Path)));
	if (NewAsset->LoadFromFile(FullPath))
	{
		NewAsset->SetSourcePath(Path);
		LoadedCurves.emplace(Path, NewAsset);
		return NewAsset;
	}
	else
	{
		UObjectManager::Get().DestroyObject(NewAsset);
		return nullptr;
	}
}

UFloatCurveAsset* FFloatCurveManager::Find(const FString& Path) const
{
	auto it = LoadedCurves.find(Path);
	return it != LoadedCurves.end() ? it->second : nullptr;
}

void FFloatCurveManager::Save(UFloatCurveAsset* Asset)
{
	if (!Asset)
	{
		return;
	}
	const FString& Path = Asset->GetSourcePath();
	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::AssetDir(), FPaths::ToWide(Path)));

	if (Path.empty())
	{
		return;
	}
	Asset->SaveToFile(FullPath);
}
