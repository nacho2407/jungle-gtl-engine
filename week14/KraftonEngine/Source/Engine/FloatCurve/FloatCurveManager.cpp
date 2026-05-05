#include "FloatCurveManager.h"
#include "FloatCurveAsset.h"

UFloatCurveAsset* FFloatCurveManager::Load(const FString& Path)
{
	auto it = LoadedCurves.find(Path);
	if (it != LoadedCurves.end())
	{
		return it->second;
	}

	UFloatCurveAsset* NewAsset = UObjectManager::Get().CreateObject<UFloatCurveAsset>();
	if (NewAsset->LoadFromFile(Path))
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
	if (Path.empty())
	{
		return;
	}
	Asset->SaveToFile(Path);
}
