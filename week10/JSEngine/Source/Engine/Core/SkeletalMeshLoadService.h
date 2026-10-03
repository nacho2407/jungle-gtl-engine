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

private:
	// FBX import 후 캐시 굽기 / binary 캐시 신선하면 직독, 둘 다 실패 시 nullptr.
	USkeletalMesh* LoadSourceOrCachedBinary(const FString& NormalizedPath);

	// 로드된 FSkeletalMesh 데이터 후처리:
	// material slot resolve → USkeletalMesh wrap → cache 등록.
	USkeletalMesh* FinalizeLoadedMesh(FSkeletalMesh* MeshData, const FString& ResolvePath, const FString& CacheKey);

	FResourceManager& ResourceManager;
};
