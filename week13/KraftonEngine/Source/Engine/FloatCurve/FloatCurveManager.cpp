#include "FloatCurveManager.h"
#include "FloatCurveAsset.h"
#include "Platform/Paths.h"

UFloatCurveAsset* FFloatCurveManager::Load(const FString& Path)
{
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto it = LoadedCurves.find(NormalizedPath);
	if (it != LoadedCurves.end())
	{
		return it->second;
	}

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(NormalizedPath)));
	if (NewAsset->LoadFromFile(FullPath))
	{
		NewAsset->SetSourcePath(NormalizedPath);
		LoadedCurves.emplace(NormalizedPath, NewAsset);
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
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto it = LoadedCurves.find(NormalizedPath);
	return it != LoadedCurves.end() ? it->second : nullptr;
}

void FFloatCurveManager::Save(UFloatCurveAsset* Asset)
{
	if (!Asset)
	{
		return;
	}
	const FString& Path = Asset->GetSourcePath();
	const FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::RootDir(), FPaths::ToWide(Path)));

	if (Path.empty())
	{
		return;
	}
	Asset->SaveToFile(FullPath);
}
