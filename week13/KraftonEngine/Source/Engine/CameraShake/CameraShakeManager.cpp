#include "CameraShakeManager.h"
#include "CameraShakeAsset.h"
#include "Platform/Paths.h"

UCameraShakeAsset* FCameraShakeManager::Load(const FString& Path)
{
	FString NormalizedPath = FPaths::MakeProjectRelative(Path);

	auto it = LoadedShakes.find(NormalizedPath);
	if (it != LoadedShakes.end())
	{
		return it->second;
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

void FCameraShakeManager::Save(UCameraShakeAsset* Asset)
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
