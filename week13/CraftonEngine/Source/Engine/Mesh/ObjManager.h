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
	static TArray<FMeshAssetListItem> AvailableMeshFiles;


public:
	static std::string GetBinaryFilePath(const std::string& OriginalPath);
	static UStaticMesh* LoadObjStaticMesh(const std::string& PathFileName, ID3D11Device* InDevice);
	static UStaticMesh* LoadBinStaticMesh(const std::string& BinFilePath, ID3D11Device* InDevice);
	static void ScanMeshAssets();
	static const TArray<FMeshAssetListItem>& GetAvailableMeshFiles();

private:
	static bool LoadStaticMeshAsset(const std::string& PathFileName, ID3D11Device* InDevice,
		std::unique_ptr<FStaticMesh>& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
