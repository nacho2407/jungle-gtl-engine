#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/Types/RenderTypes.h"

#include <map>
#include <string>
#include <memory>

struct FStaticMesh;
struct FSkeletalMesh;
struct FStaticMaterial;
struct FImportOptions;
struct FMeshAssetListItem;
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

	static USkeletalMesh* LoadSkeletalMesh(const FString& PathFileName , ID3D11Device* InDevice);
	static bool LoadSkeletalMeshAsset(const FString& PathFileName, ID3D11Device* InDevice, FSkeletalMesh*& OutMesh);

	static void ScanMeshSourceFiles();
	static void ScanFbxSourceFiles();

	static const TArray<FMeshAssetListItem>& GetAvailableStaticMeshFiles() { return AvailableMeshFiles; };
	static const TArray<FMeshAssetListItem>& GetAvailableObjFiles() { return AvailableObjFiles; };
	static const TArray<FMeshAssetListItem>& GetAvailableFbxFiles() { return AvailableFbxFiles; };
	static const TArray<FMeshAssetListItem>& GetAvailableSkeletalMeshFiles() { return AvailableFbxFiles; };

	// 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
	static void ReleaseAllGPU();
	static void ScanMeshAssets();
	static FString GetBinaryFilePath(const FString& OriginalPath);

public:
	static TMap<FString, UStaticMesh*> StaticMeshCache;
	static TMap<FString, USkeletalMesh*> SkeletalMeshCache;
	static TArray<FMeshAssetListItem> AvailableMeshFiles;
	static TArray<FMeshAssetListItem> AvailableObjFiles;
	static TArray<FMeshAssetListItem> AvailableFbxFiles;
};

