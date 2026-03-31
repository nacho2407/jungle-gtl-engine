#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/Types/RenderTypes.h"
#include <map>
#include <string>
#include <memory>

struct FStaticMesh;
struct FStaticMaterial;
class UStaticMesh;
class UMaterial;

struct FMeshAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

// 에셋 로드/캐싱 관리자
class FObjManager
{
	// path → UStaticMesh* 캐시 (소유권은 UObjectManager)
	static std::map<std::string, UStaticMesh*> StaticMeshCache;
	static TMap<FString, UMaterial*> MaterialCache;
	static TArray<FMeshAssetListItem> AvailableMeshFiles;


public:
	static std::string GetBinaryFilePath(const std::string& OriginalPath);
	static FString GetMBinaryFilePath(const FString& OriginalPath);
	static UStaticMesh* LoadObjStaticMesh(const std::string& PathFileName, ID3D11Device* InDevice);
	static UMaterial* GetOrLoadMaterial(const FString& MaterialName);
	static void ScanMeshAssets();
	static const TArray<FMeshAssetListItem>& GetAvailableMeshFiles();

private:
	static bool LoadStaticMeshAsset(const std::string& PathFileName, ID3D11Device* InDevice,
		FStaticMesh*& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
