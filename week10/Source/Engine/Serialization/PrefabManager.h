#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Paths.h"

#include <filesystem>

class AActor;
class UWorld;

class FPrefabManager
{
public:
	static constexpr const wchar_t* PrefabExtension = L".prefab";

	static std::wstring GetPrefabDirectory() { return FPaths::RootDir() + L"Asset/Prefab/"; }

	static bool SaveActorPrefab(AActor* Actor, const FString& FilePath);
	static AActor* SpawnActorFromPrefab(UWorld* World, const FString& RelativePath);

	static bool ResolvePrefabPath(const FString& Path, std::filesystem::path& OutPath, bool bForSave);
	static FString MakeRelativePrefabPath(const std::filesystem::path& AbsolutePath);
};
