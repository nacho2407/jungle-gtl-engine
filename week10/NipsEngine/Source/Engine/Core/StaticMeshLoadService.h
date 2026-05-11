#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
class UStaticMesh;
struct FStaticMesh;

class FStaticMeshLoadService
{
public:
	explicit FStaticMeshLoadService(FResourceManager& InResourceManager);

	UStaticMesh* Load(const FString& Path);

private:
	UStaticMesh* LoadMissingObjBinaryFallback(const FString& RequestedPath, const FString& BinaryPath);
	UStaticMesh* LoadBinaryDrop(const FString& NormalizedPath);

	// OBJ / FBX 등 source mesh 파일을 로드. binary cache 유효하면 그쪽 우선.
	UStaticMesh* LoadSourceOrCachedBinary(const FString& NormalizedPath);

	// 로드된 FStaticMesh 데이터의 후처리 공통부:
	// material slot resolve → UStaticMesh wrap → cache 등록 (선택적으로 secondary key 추가).
	UStaticMesh* FinalizeLoadedMesh(FStaticMesh* MeshData,
	                                const FString& ResolvePath,
	                                const FString& PrimaryCacheKey,
	                                const FString& SecondaryCacheKey,
	                                bool bLogLodTiming,
	                                bool bLogLodSkipped);

	FResourceManager& ResourceManager;
};
