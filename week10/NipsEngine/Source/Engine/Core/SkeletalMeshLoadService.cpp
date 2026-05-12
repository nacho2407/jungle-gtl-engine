#include "Core/SkeletalMeshLoadService.h"

#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

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

	ResourceManager.LoadMaterial(NormalizedPath, "Shaders/UberLit.hlsl");

	return LoadSourceOrCachedBinary(NormalizedPath);
}

USkeletalMesh* FSkeletalMeshLoadService::LoadSourceOrCachedBinary(const FString& NormalizedPath)
{
	FStaticMeshLoadOptions LoadOptions;
	const FString BinaryPath = FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(NormalizedPath);

	FSkeletalMesh* LoadedMeshData = nullptr;
	double BinaryLoadSec = 0.0;
	double SourceLoadSec = 0.0;

	// 1) Binary 캐시가 소스보다 신선하면 그걸 우선 시도.
	if (ResourceManager.IsSkeletalMeshBinaryValid(NormalizedPath, BinaryPath))
	{
		const auto BinaryStart = std::chrono::steady_clock::now();

		LoadedMeshData = new FSkeletalMesh();
		if (!ResourceManager.BinarySerializer.LoadSkeletalMesh(BinaryPath, *LoadedMeshData))
		{
			delete LoadedMeshData;
			LoadedMeshData = nullptr;
		}

		const auto BinaryEnd = std::chrono::steady_clock::now();
		BinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();
	}

	// 2) Binary 실패/누락이면 FBX에서 import 후 캐시 굽기.
	if (LoadedMeshData == nullptr)
	{
		const auto SourceStart = std::chrono::steady_clock::now();
		LoadedMeshData = ResourceManager.FbxImporter.LoadSkeletalMesh(NormalizedPath, LoadOptions);
		const auto SourceEnd = std::chrono::steady_clock::now();
		SourceLoadSec = std::chrono::duration<double>(SourceEnd - SourceStart).count();

		if (!LoadedMeshData)
		{
			UE_LOG_ERROR("[SkeletalMeshLoad] Failed | Path=%s | BinarySec=%.6f | FbxSec=%.6f",
				NormalizedPath.c_str(), BinaryLoadSec, SourceLoadSec);
			return nullptr;
		}

		// Material 포인터는 직렬화 대상이 아니므로 이 시점에 그대로 굽고, resolve는 Finalize에서 한 번만.
		const bool bSaveBinaryOk = ResourceManager.BinarySerializer.SaveSkeletalMesh(BinaryPath, NormalizedPath, *LoadedMeshData);
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
		UE_LOG("[SkeletalMeshLoad] Source=Binary | Path=%s | BinarySec=%.6f | BinaryPath=%s",
		       NormalizedPath.c_str(), BinaryLoadSec, BinaryPath.c_str());
	}

	return FinalizeLoadedMesh(LoadedMeshData, NormalizedPath, NormalizedPath);
}

USkeletalMesh* FSkeletalMeshLoadService::FinalizeLoadedMesh(FSkeletalMesh* MeshData, const FString& ResolvePath, const FString& CacheKey)
{
	ResourceManager.ResolveSkeletalMeshMaterialSlots(ResolvePath, MeshData);

	USkeletalMesh* LoadedMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	LoadedMesh->SetMeshData(MeshData);

	ResourceManager.SkeletalMeshMap[CacheKey] = LoadedMesh;
	if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), CacheKey)
		== ResourceManager.SkeletalMeshFilePaths.end())
	{
		ResourceManager.SkeletalMeshFilePaths.push_back(CacheKey);
	}

	UE_LOG("[SkeletalMeshLoad] Loaded | Path=%s | Vertices=%zu | Indices=%zu | Bones=%zu | Sections=%zu",
	       CacheKey.c_str(),
	       LoadedMesh->GetVertices().size(),
	       LoadedMesh->GetIndices().size(),
	       LoadedMesh->GetBones().size(),
	       LoadedMesh->GetSections().size());

	return LoadedMesh;
}
