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
struct FMeshAssetListItem;
struct FFbxListItem;
class UStaticMesh;
class USkeletalMesh;

struct FMeshAssetListItem
{
	FString DisplayName;
	FString FullPath;
};


class FMeshManager
{
public:
	static FMeshManager& Get();
	static UStaticMesh* LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice);
	static UStaticMesh* LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice);

	static USkeletalMesh* LoadSkeletalMesh(const FString* Path, ID3D11Device* Device);

	static void ScanMeshSourceFiles();
	static const TArray<FMeshAssetListItem>& GetAvailableStaticMeshFiles() { return AvailableMeshFiles; };
	static const TArray<FMeshAssetListItem>& GetAvailableObjFiles() { return AvailableObjFiles; };
	static const TArray<FFbxListItem>& GetAvailableSkeletalMeshFiles() { return AvailableFbxFiles; };

	// 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
	static void ReleaseAllGPU();
	static void ScanMeshAssets();
	static FString GetBinaryFilePath(const FString& OriginalPath);

public:
	static TMap<std::string, UStaticMesh*> StaticMeshCache;
	static TArray<FMeshAssetListItem> AvailableMeshFiles;
	static TArray<FMeshAssetListItem> AvailableObjFiles;
	static TArray<FFbxListItem>		  AvailableFbxFiles;
};

