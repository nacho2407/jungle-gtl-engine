#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
class USkeletalMesh;
struct FSkeletalMesh;

class FSkeletalMeshLoadService
{
public:
	explicit FSkeletalMeshLoadService(FResourceManager& InResourceManager);

	USkeletalMesh* Load(const FString& Path);
	USkeletalMesh* ImportFbxSource(const FString& Path);

private:
	USkeletalMesh* LoadImportedFbxAsset(const FString& NormalizedPath);
	USkeletalMesh* LoadBinaryAsset(const FString& NormalizedPath);
	FSkeletalMesh* TryLoadBinary(const FString& BinaryPath, double& OutBinaryLoadSec);

	// 로드된 FSkeletalMesh 데이터 후처리:
	// material slot resolve → USkeletalMesh wrap → cache 등록.
	USkeletalMesh* FinalizeLoadedMesh(FSkeletalMesh* MeshData, const FString& ResolvePath, const FString& CacheKey);

	FResourceManager& ResourceManager;
};
