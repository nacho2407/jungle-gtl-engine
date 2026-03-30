#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"

std::map<FString, UStaticMesh*> FObjManager::StaticMeshCache;

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	size_t DotPos = OriginalPath.find_last_of('.');
	if (DotPos != FString::npos)
	{
		return OriginalPath.substr(0, DotPos) + ".bin";
	}
	return OriginalPath + ".bin";
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	// 캐시 확인 (O(1) 룩업)
	auto It = StaticMeshCache.find(PathFileName);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	// UStaticMesh 생성 + FStaticMesh 소유권 이전 + 머티리얼 설정
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	StaticMesh->Build(PathFileName, InDevice);

	// 캐시 등록
	StaticMeshCache[PathFileName] = StaticMesh;

	return StaticMesh;
}