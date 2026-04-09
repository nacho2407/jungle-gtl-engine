#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/Types/RenderTypes.h"
#include <map>
#include <string>
#include <memory>

struct FStaticMesh;
struct FStaticMaterial;
struct FImportOptions;
class UStaticMesh;
class UMaterial;

struct FMeshAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

struct FMaterialAssetListItem
{
	FString DisplayName;
	FString FullPath;
};

// 에셋 로드/캐싱 관리자
class FObjManager
{
	// path → UStaticMesh* 캐시 (소유권은 UObjectManager)
	static TMap<std::string, UStaticMesh*> StaticMeshCache;
	static TMap<FString, UMaterial*> MaterialCache;
	static TArray<FMeshAssetListItem> AvailableMeshFiles;
	static TArray<FMeshAssetListItem> AvailableObjFiles;
	static TArray< FMaterialAssetListItem> AvailableMaterialFiles;


public:
	static std::string GetBinaryFilePath(const std::string& OriginalPath);
	static FString GetMBinaryFilePath(const FString& OriginalPath);
	static UStaticMesh* LoadObjStaticMesh(const std::string& PathFileName, ID3D11Device* InDevice);
	static UStaticMesh* LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice);
	static UMaterial* GetOrLoadMaterial(const FString& MaterialName);
	static void ScanMeshAssets();
	static const TArray<FMeshAssetListItem>& GetAvailableMeshFiles();
	static void ScanObjSourceFiles();
	static const TArray<FMeshAssetListItem>& GetAvailableObjFiles();
	static void ScanMaterialAssets();
	static const TArray<FMaterialAssetListItem>& GetAvailableMaterialFiles();

private:
	static bool LoadStaticMeshAsset(const std::string& PathFileName, ID3D11Device* InDevice,
		FStaticMesh*& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
