#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Materials/Material.h"
#include "Serialization/WindowsArchive.h"
#include <filesystem>

std::map<FString, UStaticMesh*> FObjManager::StaticMeshCache;
TMap<FString, UMaterial*> FObjManager::MaterialCache;

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

UMaterial* FObjManager::GetOrLoadMaterial(const FString& MaterialName)
{
	// 1. 캐시(RAM)에 이미 있는지 검사
	if (MaterialCache.contains(MaterialName))
	{
		return MaterialCache[MaterialName];
	}

	// 2. 캐시에 없다면 빈 객체 생성
	UMaterial* NewMaterial = UObjectManager::Get().CreateObject<UMaterial>();
	FString MatPath = MaterialName;

	// 3. 하드디스크(.bin)에 있다면 로드
	if (std::filesystem::exists(MatPath))
	{
		FWindowsBinReader Reader(MatPath);
		if (Reader.IsValid())
		{
			NewMaterial->Serialize(Reader);
		}
	}

	// 4. 캐시에 등록 후 반환
	MaterialCache[MaterialName] = NewMaterial;
	return NewMaterial;
}