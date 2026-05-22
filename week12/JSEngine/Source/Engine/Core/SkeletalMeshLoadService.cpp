#include "Core/SkeletalMeshLoadService.h"

#include "Core/AssetPathPolicy.h"
#include "Core/Logging/Log.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>

namespace
{
bool HasUsableSkeletalMeshData(const FSkeletalMesh* MeshData)
{
	return MeshData != nullptr &&
		!MeshData->Vertices.empty() &&
		!MeshData->Indices.empty() &&
		!MeshData->Bones.empty();
}
}

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

	std::filesystem::path RequestedPath(FPaths::ToWide(NormalizedPath));
	std::wstring RequestedExtension = RequestedPath.extension().wstring();
	std::transform(RequestedExtension.begin(), RequestedExtension.end(), RequestedExtension.begin(), ::towlower);
	if (RequestedExtension == L".bin")
	{
		return LoadBinaryAsset(NormalizedPath);
	}

	return LoadImportedFbxAsset(NormalizedPath);
}

USkeletalMesh* FSkeletalMeshLoadService::LoadImportedFbxAsset(const FString& NormalizedPath)
{
	const FString BinaryPath = FAssetPathPolicy::MakeImportedSkeletalMeshAssetPath(NormalizedPath);
	double BinaryLoadSec = 0.0;
	if (!ResourceManager.IsImportedSkeletalMeshAssetFresh(NormalizedPath, BinaryPath))
	{
		return nullptr;
	}

	FSkeletalMesh* LoadedMeshData = TryLoadBinary(BinaryPath, BinaryLoadSec);
	if (LoadedMeshData == nullptr)
	{
		UE_LOG_WARNING("[SkeletalMeshLoad] MeshSource=OutdatedImportedAsset | Path=%s | Asset=%s",
			NormalizedPath.c_str(),
			BinaryPath.c_str());
		return nullptr;
	}

	ResourceManager.LoadMaterial(NormalizedPath, EMaterialShaderType::SurfaceLit);
	LoadedMeshData->PathFileName = BinaryPath;
	UE_LOG("[SkeletalMeshLoad] MeshSource=ImportedAsset | Path=%s | BinarySec=%.6f | Asset=%s",
		NormalizedPath.c_str(),
		BinaryLoadSec,
		BinaryPath.c_str());
	USkeletalMesh* LoadedMesh = FinalizeLoadedMesh(LoadedMeshData, NormalizedPath, NormalizedPath);
	ResourceManager.SkeletalMeshMap[BinaryPath] = LoadedMesh;
	if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), BinaryPath)
		== ResourceManager.SkeletalMeshFilePaths.end())
	{
		ResourceManager.SkeletalMeshFilePaths.push_back(BinaryPath);
	}
	return LoadedMesh;
}

USkeletalMesh* FSkeletalMeshLoadService::LoadBinaryAsset(const FString& NormalizedPath)
{
	double BinaryLoadSec = 0.0;
	FSkeletalMesh* LoadedMeshData = TryLoadBinary(NormalizedPath, BinaryLoadSec);
	if (LoadedMeshData == nullptr)
	{
		UE_LOG_WARNING("[SkeletalMeshLoad] MeshSource=OutdatedImportedAsset | Path=%s | Asset=%s",
			NormalizedPath.c_str(),
			NormalizedPath.c_str());
		return nullptr;
	}

	const FString SourcePath = FPaths::Normalize(LoadedMeshData->PathFileName);
	if (!SourcePath.empty())
	{
		if (USkeletalMesh* FoundSourceMesh = ResourceManager.FindSkeletalMesh(SourcePath))
		{
			delete LoadedMeshData;
			ResourceManager.SkeletalMeshMap[NormalizedPath] = FoundSourceMesh;
			if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), NormalizedPath)
				== ResourceManager.SkeletalMeshFilePaths.end())
			{
				ResourceManager.SkeletalMeshFilePaths.push_back(NormalizedPath);
			}
			return FoundSourceMesh;
		}

		ResourceManager.LoadMaterial(SourcePath, EMaterialShaderType::SurfaceLit);
	}

	LoadedMeshData->PathFileName = NormalizedPath;
	USkeletalMesh* LoadedMesh = FinalizeLoadedMesh(
		LoadedMeshData,
		SourcePath.empty() ? NormalizedPath : SourcePath,
		NormalizedPath);

	if (!SourcePath.empty() && SourcePath != NormalizedPath)
	{
		ResourceManager.SkeletalMeshMap[SourcePath] = LoadedMesh;
		if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), SourcePath)
			== ResourceManager.SkeletalMeshFilePaths.end())
		{
			ResourceManager.SkeletalMeshFilePaths.push_back(SourcePath);
		}
	}

	UE_LOG("[SkeletalMeshLoad] MeshSource=ImportedAsset | Path=%s | BinarySec=%.6f | Asset=%s | Source=%s",
		NormalizedPath.c_str(),
		BinaryLoadSec,
		NormalizedPath.c_str(),
		SourcePath.c_str());
	return LoadedMesh;
}

USkeletalMesh* FSkeletalMeshLoadService::ImportFbxSource(const FString& Path)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty())
	{
		return nullptr;
	}

	ResourceManager.ImportMaterialFromFbx(NormalizedPath, EMaterialShaderType::SurfaceLit);

	FStaticMeshLoadOptions LoadOptions;
	const FString BinaryPath = FAssetPathPolicy::MakeImportedSkeletalMeshAssetPath(NormalizedPath);

	const auto SourceStart = std::chrono::steady_clock::now();
	FSkeletalMesh* LoadedMeshData = ResourceManager.FbxImporter.LoadSkeletalMesh(NormalizedPath, LoadOptions);
	const auto SourceEnd = std::chrono::steady_clock::now();
	const double SourceLoadSec = std::chrono::duration<double>(SourceEnd - SourceStart).count();

	if (!HasUsableSkeletalMeshData(LoadedMeshData))
	{
		delete LoadedMeshData;
		UE_LOG_ERROR("[SkeletalMeshLoad] MeshSource=ExplicitFbxImport | Result=Failed | Path=%s | FbxSec=%.6f",
			NormalizedPath.c_str(),
			SourceLoadSec);
		return nullptr;
	}

	const bool bSaveBinaryOk = ResourceManager.BinarySerializer.SaveSkeletalMesh(BinaryPath, NormalizedPath, *LoadedMeshData);
	if (bSaveBinaryOk)
	{
		UE_LOG("[SkeletalMeshLoad] MeshSource=ExplicitFbxImport | Path=%s | FbxSec=%.6f | AssetSave=OK | Asset=%s",
			NormalizedPath.c_str(),
			SourceLoadSec,
			BinaryPath.c_str());
	}
	else
	{
		UE_LOG_WARNING("[SkeletalMeshLoad] MeshSource=ExplicitFbxImport | Path=%s | FbxSec=%.6f | AssetSave=FAIL | Asset=%s",
			NormalizedPath.c_str(),
			SourceLoadSec,
			BinaryPath.c_str());
	}

	LoadedMeshData->PathFileName = BinaryPath;
	USkeletalMesh* LoadedMesh = FinalizeLoadedMesh(LoadedMeshData, NormalizedPath, NormalizedPath);
	ResourceManager.SkeletalMeshMap[BinaryPath] = LoadedMesh;
	if (std::find(ResourceManager.SkeletalMeshFilePaths.begin(), ResourceManager.SkeletalMeshFilePaths.end(), BinaryPath)
		== ResourceManager.SkeletalMeshFilePaths.end())
	{
		ResourceManager.SkeletalMeshFilePaths.push_back(BinaryPath);
	}
	return LoadedMesh;
}

FSkeletalMesh* FSkeletalMeshLoadService::TryLoadBinary(const FString& BinaryPath, double& OutBinaryLoadSec)
{
	const auto BinaryStart = std::chrono::steady_clock::now();

	FSkeletalMesh* LoadedMeshData = new FSkeletalMesh();
	if (!ResourceManager.BinarySerializer.LoadSkeletalMesh(BinaryPath, *LoadedMeshData))
	{
		delete LoadedMeshData;
		LoadedMeshData = nullptr;
	}
	else if (!HasUsableSkeletalMeshData(LoadedMeshData))
	{
		delete LoadedMeshData;
		LoadedMeshData = nullptr;
	}

	const auto BinaryEnd = std::chrono::steady_clock::now();
	OutBinaryLoadSec = std::chrono::duration<double>(BinaryEnd - BinaryStart).count();
	return LoadedMeshData;
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
