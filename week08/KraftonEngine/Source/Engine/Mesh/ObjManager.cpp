#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Materials/Material.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Editor/UI/EditorConsolePanel.h"
#include <filesystem>
#include <algorithm>
#include <cwctype>

// 로드된 UStaticMesh를 바이너리 캐시 경로 기준으로 보관하는 런타임 캐시
TMap<FString, UStaticMesh*> FObjManager::StaticMeshCache;

// 에디터/UI에서 보여줄 수 있는 .bin 메쉬 캐시 목록
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;

// 에디터/UI에서 보여줄 수 있는 원본 .obj 목록
TArray<FMeshAssetListItem> FObjManager::AvailableObjFiles;

namespace
{
	static std::filesystem::path NormalizePathForCompare(const FString& InPath)
	{
		std::filesystem::path Path = FPaths::ToPath(InPath).lexically_normal();
		std::error_code Ec;
		std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
		return Ec ? Path : Canonical;
	}

	static bool IsBinExtension(const std::filesystem::path& Path)
	{
		std::wstring Ext = Path.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		return Ext == L".bin";
	}

	static bool IsObjExtension(const std::filesystem::path& Path)
	{
		std::wstring Ext = Path.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		return Ext == L".obj";
	}

	static FString BuildSiblingCachePath(const FString& OriginalPath)
	{
		const std::filesystem::path SourcePath = NormalizePathForCompare(OriginalPath);
		const std::filesystem::path CacheDir = SourcePath.parent_path() / L"Cache";
		FPaths::CreateDir(CacheDir.wstring());

		std::filesystem::path CacheFile = CacheDir / SourcePath.filename();
		CacheFile.replace_extension(L".bin");
		return FPaths::FromPath(CacheFile.lexically_normal());
	}

	static bool IsInsideCacheFolder(const std::filesystem::path& Path)
	{
		for (const auto& Part : Path)
		{
			std::wstring Name = Part.wstring();
			std::transform(Name.begin(), Name.end(), Name.begin(), ::towlower);
			if (Name == L"cache")
			{
				return true;
			}
		}
		return false;
	}
}

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	const std::filesystem::path SrcPath = NormalizePathForCompare(OriginalPath);
	if (IsBinExtension(SrcPath))
	{
		return FPaths::FromPath(SrcPath);
	}

	return BuildSiblingCachePath(OriginalPath);
}

void FObjManager::ScanMeshAssets()
{
	AvailableMeshFiles.clear();

	const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
	if (!std::filesystem::exists(AssetRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		const std::filesystem::path& Path = Entry.path();
		if (!IsBinExtension(Path) || !IsInsideCacheFolder(Path))
		{
			continue;
		}

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMeshFiles.push_back(std::move(Item));
	}
}

void FObjManager::ScanObjSourceFiles()
{
	AvailableObjFiles.clear();

	const std::filesystem::path AssetRoot = std::filesystem::path(FPaths::RootDir()) / L"Asset";
	if (!std::filesystem::exists(AssetRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
	{
		if (!Entry.is_regular_file())
		{
			continue;
		}

		const std::filesystem::path& Path = Entry.path();
		if (!IsObjExtension(Path) || IsInsideCacheFolder(Path))
		{
			continue;
		}

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableObjFiles.push_back(std::move(Item));
	}
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableMeshFiles()
{
	return AvailableMeshFiles;
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableObjFiles()
{
	return AvailableObjFiles;
}

bool FObjManager::TryImportStaticMesh(const FString& ObjPath, const FImportOptions* Options, UStaticMesh* StaticMesh, const FString& BinPath)
{
	if (!StaticMesh)
	{
		return false;
	}

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;

	const bool bImported = Options
		? FObjImporter::Import(ObjPath, *Options, *NewMeshAsset, ParsedMaterials)
		: FObjImporter::Import(ObjPath, *NewMeshAsset, ParsedMaterials);

	if (!bImported)
	{
		UE_LOG("[ERROR] Failed to import static mesh source: %s", ObjPath.c_str());
		return false;
	}

	NewMeshAsset->PathFileName = ObjPath;
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	FWindowsBinWriter Writer(BinPath);
	if (Writer.IsValid())
	{
		StaticMesh->Serialize(Writer);
	}
	else
	{
		UE_LOG("[WARN] Failed to open mesh cache for write: %s", BinPath.c_str());
	}

	return true;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	const FString CacheKey = GetBinaryFilePath(PathFileName);
	StaticMeshCache.erase(CacheKey);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	if (!TryImportStaticMesh(PathFileName, &Options, StaticMesh, CacheKey))
	{
		return nullptr;
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();
	return StaticMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const FString CacheKey = GetBinaryFilePath(PathFileName);

	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	const FString BinPath = CacheKey;
	bool bNeedRebuild = true;

	const std::filesystem::path BinPathW = FPaths::ToPath(BinPath);
	const std::filesystem::path SourcePathW = FPaths::ToPath(PathFileName);

	if (std::filesystem::exists(BinPathW))
	{
		if (!std::filesystem::exists(SourcePathW) || FPaths::FromPath(SourcePathW) == BinPath ||
			std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(SourcePathW))
		{
			bNeedRebuild = false;
		}
	}

	if (!bNeedRebuild)
	{
		FWindowsBinReader Reader(BinPath);
		if (Reader.IsValid())
		{
			StaticMesh->Serialize(Reader);
		}
		else
		{
			UE_LOG("[WARN] Failed to open mesh cache for read: %s", BinPath.c_str());
			bNeedRebuild = true;
		}
	}

	if (bNeedRebuild)
	{
		if (!TryImportStaticMesh(PathFileName, nullptr, StaticMesh, BinPath))
		{
			return nullptr;
		}
	}

	if (!StaticMesh->GetStaticMeshAsset())
	{
		UE_LOG("[ERROR] Static mesh asset was empty after load: %s", PathFileName.c_str());
		return nullptr;
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();
	return StaticMesh;
}

void FObjManager::ReleaseAllGPU()
{
	for (auto& [Key, Mesh] : StaticMeshCache)
	{
		if (!Mesh)
		{
			continue;
		}

		FStaticMesh* Asset = Mesh->GetStaticMeshAsset();
		if (Asset && Asset->RenderBuffer)
		{
			Asset->RenderBuffer->Release();
			Asset->RenderBuffer.reset();
		}

		for (uint32 LOD = 1; LOD < UStaticMesh::MAX_LOD_COUNT; ++LOD)
		{
			FMeshBuffer* LODBuffer = Mesh->GetLODMeshBuffer(LOD);
			if (LODBuffer)
			{
				LODBuffer->Release();
			}
		}
	}

	StaticMeshCache.clear();
}
