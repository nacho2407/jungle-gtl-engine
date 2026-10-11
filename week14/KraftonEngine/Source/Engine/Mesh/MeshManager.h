#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/ObjectIterator.h"
#include "Render/Types/RenderTypes.h"
#include "Asset/AssetRegistry.h"
#include "Animation/Skeleton/SkeletonTypes.h"

#include <map>
#include <string>
#include <memory>

struct FStaticMesh;
struct FSkeletalMesh;
struct FStaticMaterial;
struct FSkeletalMaterial;
struct FImportOptions;
class UStaticMesh;
class USkeletalMesh;
class USkeleton;
class UAnimSequence;
class FReferenceCollector;

struct FSkeletalMeshImportRequest
{
	FString SourceFbxPath;
	FString TargetSkeletonPath = "None";
	FString DestinationPackagePath;
	bool    bAllowTargetExtraBones   = false;
	bool    bOverwriteExistingAssets = true;
};

struct FFbxSceneImportRequest
{
	FString     SourceFbxPath;
	FString     TargetSkeletonPath = "None";
	FString     DestinationPackagePath;
	FString     DestinationAnimationDirectory;
	bool        bImportSkeleton          = true;
	bool        bImportSkin              = true;
	bool        bImportAnimations        = true;
	bool        bAllowTargetExtraBones   = false;
	bool        bOverwriteExistingAssets = true;
	TSet<int32> SelectedAnimationStackIndices;
};

struct FFbxSceneImportResult
{
	USkeleton*             Skeleton     = nullptr;
	USkeletalMesh*         SkeletalMesh = nullptr;
	TArray<UAnimSequence*> AnimSequences;
};


class FMeshManager
{
public:
	static FMeshManager& Get();
	static UStaticMesh* LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice);
	static UStaticMesh* LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice);

	static USkeletalMesh* LoadSkeletalMesh(const FString& PathFileName , ID3D11Device* InDevice);
	static bool           ImportSkeletalMeshAsNew(const FString& SourceFbxPath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh);
	static bool ImportSkeletonAsNew(const FString& SourceFbxPath, USkeleton*& OutSkeleton);
	static bool           ImportSkeletalMesh(const FSkeletalMeshImportRequest& Request, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh);
	static bool ImportFbxScene(
		const FFbxSceneImportRequest& Request,
		ID3D11Device*                 Device,
		FFbxSceneImportResult&        OutResult
		);
	static bool           ReadSkeletalMeshBinding(const FString& PackagePath, FSkeletonBinding& OutBinding);
	static bool SaveSkeletalMeshPackage(USkeletalMesh* SkeletalMesh);
	static void ScanMeshSourceFiles();
	static void ScanFbxSourceFiles();

	static const TArray<FAssetListItem>& GetAvailableStaticMeshFiles() { return AvailableStaticMeshFiles; };
	static const TArray<FAssetListItem>& GetAvailableSkeletalMeshFiles() { return AvailableSkeletalMeshFiles; };
	static const TArray<FAssetListItem>& GetAvailableObjFiles() { return AvailableStaticMeshSourceFiles; }
	static const TArray<FAssetListItem>& GetAvailableFbxFiles() { return AvailableFbxSourceFiles; }

	// 캐시된 StaticMesh GPU 리소스 해제 (Shutdown 시 Device 해제 전 호출)
	static void ReleaseAllGPU();
	static void ScanMeshAssets();
	static FString GetStaticMeshBinaryFilePath(const FString& SourcePath);
	static FString GetSkeletalMeshBinaryFilePath(const FString& SourcePath);
	static bool IsAssetPackagePath(const FString& Path);

	static bool ReimportStaticMesh(const FString& BinaryPath, ID3D11Device* Device, UStaticMesh*& OutStaticMesh);
	static bool ReimportSkeletalMesh(const FString& BinaryPath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh);

	static bool IsStaticMeshPackage(const FString& Path);
	static bool IsSkeletalMeshPackage(const FString& Path);

	// GC
	static void AddReferencedObjects(FReferenceCollector& Collector);
public:
	static TMap<FString, UStaticMesh*> StaticMeshCache;
	static TMap<FString, USkeletalMesh*> SkeletalMeshCache;
	static TArray<FAssetListItem> AvailableStaticMeshFiles;
	static TArray<FAssetListItem> AvailableStaticMeshSourceFiles;
	static TArray<FAssetListItem> AvailableSkeletalMeshFiles;
	static TArray<FAssetListItem> AvailableFbxSourceFiles;
};

