#pragma once
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"

struct FSkeletalMesh;
class USkeletalMesh;

struct FFbxListItem
{
	FString DisplayName;
	FString FullPath;
};

class FFbxManager
{
	static TMap<FString, USkeletalMesh*> SkeletalMeshCache;
	static TArray<FFbxListItem> AvailableFbxFiles;

public:
	static USkeletalMesh* LoadFbxSkeletalMesh(const FString& FilePath, ID3D11Device* InDevice);
	static void ScanFbxSourceFiles();
	static const TArray<FFbxListItem>& GetAvailableFbxFiles() { return AvailableFbxFiles; }

private:
	static bool LoadSkeletalMeshAsset(const FString& FilePath, ID3D11Device* InDevice,
		FSkeletalMesh*& OutMesh);
};