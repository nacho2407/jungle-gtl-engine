#include "Core/SkeletalMeshLoadService.h"

#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Asset/Skeleton.h"
#include "Asset/SkeletalMeshTypes.h"

#include <algorithm>
#include <chrono>

FSkeletalMeshLoadService::FSkeletalMeshLoadService(FResourceManager& InResourceManager)
	: ResourceManager(InResourceManager)
{
}

USkeletalMesh* FSkeletalMeshLoadService::Load(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);

	if (USkeletalMesh* FoundMesh = ResourceManager.FindSkeletalMesh(NormalizedPath))
	{
		return FoundMesh;
	}

	return LoadSourceOrCachedBinary(NormalizedPath);
}

USkeletalMesh* FSkeletalMeshLoadService::LoadSourceOrCachedBinary(const FString& NormalizedPath)
{
	FStaticMeshLoadOptions LoadOptions;
	const FString BinaryPath = FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(NormalizedPath);

	FSkeletalMesh* LoadedMeshData = nullptr;
	FReferenceSkeleton LoadedReferenceSkeleton;
	double BinaryLoadSec = 0.0;
	double SourceLoadSec = 0.0;

	// 1) Binary 캐시가 소스보다 신선하면 그걸 우선 시도.
	if (ResourceManager.IsSkeletalMeshBinaryValid(NormalizedPath, BinaryPath))
	{
		const auto BinaryStart = std::chrono::steady_clock::now();

		LoadedMeshData = new FSkeletalMesh();
		if (!ResourceManager.BinarySerializer.LoadSkeletalMesh(BinaryPath, *LoadedMeshData, LoadedReferenceSkeleton))
		{
			delete LoadedMeshData;
			LoadedMeshData = nullptr;
			LoadedReferenceSkeleton = {};
		}

		const auto BinaryEnd = std::chrono::steady_clock::now();
		BinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();
	}

	// 2) Binary 실패/누락이면 FBX에서 import 후 캐시 굽기.
	if (LoadedMeshData == nullptr)
	{
		const auto SourceStart = std::chrono::steady_clock::now();
        FSkeletalMeshImportData ImportData;
        if (!ResourceManager.FbxImporter.LoadSkeletalMesh(NormalizedPath, LoadOptions, ImportData))
        {
            return nullptr;
        }
        LoadedMeshData = ImportData.MeshData;
		LoadedReferenceSkeleton = ImportData.ReferenceSkeleton;

		const auto SourceEnd = std::chrono::steady_clock::now();
		SourceLoadSec = std::chrono::duration<double>(SourceEnd - SourceStart).count();

		if (!LoadedMeshData)
		{
			UE_LOG_ERROR("[SkeletalMeshLoad] Failed | Path=%s | BinarySec=%.6f | FbxSec=%.6f",
				NormalizedPath.c_str(), BinaryLoadSec, SourceLoadSec);
			return nullptr;
		}

		// Material 포인터는 직렬화 대상이 아니므로 이 시점에 그대로 굽고, resolve는 Finalize에서 한 번만.
		ResourceManager.LoadMaterial(NormalizedPath, EMaterialShaderType::SurfaceLit, nullptr, /*bAllowSourceImport=*/ true);

		const bool bSaveBinaryOk = ResourceManager.BinarySerializer.SaveSkeletalMesh(BinaryPath, NormalizedPath, *LoadedMeshData, LoadedReferenceSkeleton);
		if (bSaveBinaryOk)
		{
			UE_LOG("[SkeletalMeshLoad] Source=FBX | Path=%s | FbxSec=%.6f | BinarySave=OK | BinaryPath=%s",
			       NormalizedPath.c_str(), SourceLoadSec, BinaryPath.c_str());
		}
		else
		{
			UE_LOG_WARNING("[SkeletalMeshLoad] Source=FBX | Path=%s | FbxSec=%.6f | BinarySave=FAIL | BinaryPath=%s",
			               NormalizedPath.c_str(), SourceLoadSec, BinaryPath.c_str());
		}
	}
	else
	{
		ResourceManager.LoadMaterial(NormalizedPath, EMaterialShaderType::SurfaceLit, nullptr, /*bAllowSourceImport=*/ false);

		UE_LOG("[SkeletalMeshLoad] Source=Binary | Path=%s | BinarySec=%.6f | BinaryPath=%s",
		       NormalizedPath.c_str(), BinaryLoadSec, BinaryPath.c_str());
	}

	return FinalizeLoadedMesh(LoadedMeshData, LoadedReferenceSkeleton, NormalizedPath, NormalizedPath);
}

USkeletalMesh* FSkeletalMeshLoadService::FinalizeLoadedMesh(FSkeletalMesh* MeshData, FReferenceSkeleton ReferenceSkeleton, const FString& ResolvePath, const FString& CacheKey)
{
	ResourceManager.ResolveSkeletalMeshMaterialSlots(ResolvePath, MeshData);

	USkeletalMesh* LoadedMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	ReferenceSkeleton.RebuildNameToIndex();
	USkeleton* Skeleton = UObjectManager::Get().CreateObject<USkeleton>();
	Skeleton->SetReferenceSkeleton(ReferenceSkeleton);
	LoadedMesh->SetSkeleton(Skeleton, true);
	LoadedMesh->SetMeshData(MeshData);

	ResourceManager.SkeletalMeshMap[CacheKey] = LoadedMesh;
	if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), CacheKey)
		== ResourceManager.SkeletalMeshFilePaths.end())
	{
		ResourceManager.SkeletalMeshFilePaths.push_back(CacheKey);
	}

	size_t MaxSectionBoneCount = 0;
	size_t TotalSectionBoneCount = 0;
	for (const FSkeletalMeshRenderSection& Section : LoadedMesh->GetRenderSections())
	{
		MaxSectionBoneCount = std::max(MaxSectionBoneCount, Section.BoneMap.size());
		TotalSectionBoneCount += Section.BoneMap.size();
	}

	const size_t SectionCount = LoadedMesh->GetRenderSections().size();
	const double AverageSectionBoneCount = SectionCount > 0
		? static_cast<double>(TotalSectionBoneCount) / static_cast<double>(SectionCount)
		: 0.0;

	UE_LOG("[SkeletalMeshLoad] Loaded | Path=%s | Vertices=%zu | Indices=%zu | Bones=%zu | Sections=%zu | MaxSectionBones=%zu | AvgSectionBones=%.2f",
	       CacheKey.c_str(),
	       LoadedMesh->GetVertices().size(),
	       LoadedMesh->GetIndices().size(),
	       LoadedMesh->GetBones().size(),
	       SectionCount,
	       MaxSectionBoneCount,
	       AverageSectionBoneCount);

	return LoadedMesh;
}
