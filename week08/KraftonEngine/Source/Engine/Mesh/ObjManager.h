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

class FObjManager
{
	// path → UStaticMesh* 캐시 (소유권은 UObjectManager)
	static TMap<std::string, UStaticMesh*> StaticMeshCache;
	static TArray<FMeshAssetListItem> AvailableMeshFiles;
	static TArray<FMeshAssetListItem> AvailableObjFiles;


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

	// 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
	static void ReleaseAllGPU();

private:
	static bool LoadStaticMeshAsset(const std::string& PathFileName, ID3D11Device* InDevice,
		FStaticMesh*& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
