#include "MeshManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/MeshBinary.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Asset/AssetPackage.h"

#include <algorithm>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <memory>

TMap<FString, UStaticMesh*> FMeshManager::StaticMeshCache;
TMap<FString, USkeletalMesh*> FMeshManager::SkeletalMeshCache;
TArray<FMeshAssetListItem> FMeshManager::AvailableStaticMeshFiles;
TArray<FMeshAssetListItem> FMeshManager::AvailableStaticMeshSourceFiles;
TArray<FMeshAssetListItem> FMeshManager::AvailableSkeletalMeshFiles;
TArray<FMeshAssetListItem> FMeshManager::AvailableFbxSourceFiles;

FMeshManager& FMeshManager::Get()
{
	static FMeshManager Instance;
	return Instance;
}

static void EnsureMeshCacheDirExists()
{
	static bool bCreated = false;
	if (!bCreated)
	{
		std::wstring CacheDir = FPaths::RootDir() + L"Asset/MeshCache/";
		FPaths::CreateDir(CacheDir);
		bCreated = true;
	}
}

static std::wstring GetLowerExtension(const FString& Path)
{
	std::filesystem::path SrcPath(FPaths::ToWide(Path));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
	return Ext;
}

static FString NormalizeProjectPath(const FString& Path)
{
	return FPaths::MakeProjectRelative(Path);
}

static FString GetMeshPackageFilePath(const FString& SourcePath, EAssetPackageType Type)
{
	std::filesystem::path SrcPath(FPaths::ToWide(SourcePath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	if (Ext == MeshBinary::AssetPackageExtension)
	{
		return NormalizeProjectPath(SourcePath);
	}

	// 경로는 동일하지만, 확장자를 나눠서 Mesh 타입을 알 수 있게 한다.
	std::filesystem::path ProjectRelative = std::filesystem::path(FPaths::ToWide(FPaths::MakeProjectRelative(SourcePath))).lexically_normal();

	std::filesystem::path AssetPath = std::filesystem::path(L"Content") / ProjectRelative;

	if (Type == EAssetPackageType::StaticMesh)
	{
		AssetPath.replace_filename(AssetPath.stem().wstring() + L"_StaticMesh" + L".uasset");
	}
	else if (Type == EAssetPackageType::SkeletalMesh)
	{
		AssetPath.replace_filename(AssetPath.stem().wstring() + L"_SkeletalMesh" + L".uasset");
	}
	else
	{
		UE_LOG("GetMeshPackageFilePath failed: unsupported asset package type. SourcePath=%s", SourcePath.c_str());
		return FString();
	}

	std::filesystem::path FullAssetPath = std::filesystem::path(FPaths::RootDir()) / AssetPath;

	FPaths::CreateDir(FullAssetPath.parent_path().wstring());

	return FPaths::ToUtf8(AssetPath.generic_wstring());
}

static std::filesystem::path ResolveProjectPath(const FString& Path)
{
	std::filesystem::path FullPath(FPaths::ToWide(Path));
	if (!FullPath.is_absolute())
	{
		FullPath = std::filesystem::path(FPaths::RootDir()) / FullPath;
	}
	return FullPath.lexically_normal();
}

static bool TryGetSourceFileState(const FString& SourcePath, uint64& OutTimestamp, uint64& OutFileSize)
{
	std::filesystem::path FullPath = ResolveProjectPath(SourcePath);

	if (!std::filesystem::exists(FullPath) || !std::filesystem::is_regular_file(FullPath)) return false;

	OutFileSize = static_cast<uint64>(std::filesystem::file_size(FullPath));

	const auto WriteTime = std::filesystem::last_write_time(FullPath);
	OutTimestamp = static_cast<uint64>(WriteTime.time_since_epoch().count());

	return true;
}

static FAssetImportMetadata MakeImportMetadata(const FString& SourcePath)
{
	FAssetImportMetadata Metadata;
	Metadata.SourcePath = NormalizeProjectPath(SourcePath);

	TryGetSourceFileState(SourcePath, Metadata.SourceTimestamp, Metadata.SourceFileSize);

	return Metadata;
}

static bool LoadPackageMetadata(const FString& BinaryPath, EAssetPackageType ExpectedType, FAssetImportMetadata& OutMetadata)
{
	FWindowsBinReader Reader(BinaryPath);
	if (!Reader.IsValid()) return false;

	FAssetPackageHeader Header;
	Reader << Header;

	if (!Header.IsValid(ExpectedType)) return false;

	Reader << OutMetadata;
	return Reader.IsValid();
}

static bool IsPackageSourceStale(const FString& BinaryPath, EAssetPackageType ExpectedType, bool& bOutMissingSource)
{
	bOutMissingSource = false;

	FAssetImportMetadata Metadata;
	if (!LoadPackageMetadata(BinaryPath, ExpectedType, Metadata)) return true;

	uint64 CurrentTimestamp = 0;
	uint64 CurrentFileSize = 0;
	if (!TryGetSourceFileState(Metadata.SourcePath, CurrentTimestamp, CurrentFileSize))
	{
		bOutMissingSource = true;
		return true;
	}

	return !Metadata.MatchesSource(CurrentTimestamp, CurrentFileSize);
}

static bool IsSupportedStaticMeshSourcePath(const FString& Path)
{
	const std::wstring Ext = GetLowerExtension(Path);
	return Ext == L".obj" || Ext == L".fbx";
}

static bool IsSupportedSkeletalMeshSourcePath(const FString& Path)
{
	const std::wstring Ext = GetLowerExtension(Path);
	return Ext == L".fbx";
}

static bool ImportStaticMeshByExtension(const FString& PathFileName, const FImportOptions* Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	const std::wstring Ext = GetLowerExtension(PathFileName);
	if (Ext == L".obj")
	{
		return Options
			? FObjImporter::Import(PathFileName, *Options, OutMesh, OutMaterials)
			: FObjImporter::Import(PathFileName, OutMesh, OutMaterials);
	}

	if (Ext == L".fbx")
	{
		return FFbxImporter::ImportStatic(PathFileName, Options, OutMesh, OutMaterials);
	}

	UE_LOG("StaticMesh import failed: unsupported source extension. Path=%s", PathFileName.c_str());
	return false;
}

static bool LoadStaticMeshBinary(UStaticMesh* StaticMesh, const FString& BinaryPath)
{
	FWindowsBinReader Reader(BinaryPath);
	if (!Reader.IsValid())
	{
		UE_LOG("StaticMesh binary open failed. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Reader << Header;

		if (!Header.IsValid(EAssetPackageType::StaticMesh))
		{
			UE_LOG("StaticMesh binary read failed: invalid file header. Path=%s", BinaryPath.c_str());
			return false;
		}

		FAssetImportMetadata Metadata;
		Reader << Metadata;

		StaticMesh->Serialize(Reader);
	}
	catch (const std::exception&)
	{
		UE_LOG("StaticMesh binary read failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	if (!Reader.IsValid())
	{
		UE_LOG("StaticMesh binary read failed: file data is incomplete or corrupted. Path=%s", BinaryPath.c_str());
		return false;
	}

	return true;
}

static bool SaveStaticMeshBinary(UStaticMesh* StaticMesh, const FString& BinaryPath, const FString& SourcePath)
{
	FWindowsBinWriter Writer(BinaryPath);
	if (!Writer.IsValid())
	{
		UE_LOG("StaticMesh binary save failed: could not open file. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Header.Type = static_cast<uint32>(EAssetPackageType::StaticMesh);
		Writer << Header;

		FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
		Writer << Metadata;

		StaticMesh->Serialize(Writer);
	}
	catch (const std::exception&)
	{
		UE_LOG("StaticMesh binary save failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	return Writer.IsValid();
}

static bool LoadSkeletalMeshBinary(USkeletalMesh* SkeletalMesh, const FString& BinaryPath)
{
	FWindowsBinReader Reader(BinaryPath);
	if (!Reader.IsValid())
	{
		UE_LOG("SkeletalMesh binary open failed. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Reader << Header;

		if (!Header.IsValid(EAssetPackageType::SkeletalMesh))
		{
			UE_LOG("SkeletalMesh binary read failed: invalid file header. Path=%s", BinaryPath.c_str());
			return false;
		}

		FAssetImportMetadata Metadata;
		Reader << Metadata;

		SkeletalMesh->Serialize(Reader);
	}
	catch (const std::exception&)
	{
		UE_LOG("SkeletalMesh binary read failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	if (!Reader.IsValid())
	{
		UE_LOG("SkeletalMesh binary read failed: file data is incomplete or corrupted. Path=%s", BinaryPath.c_str());
		return false;
	}

	return true;
}

static bool SaveSkeletalMeshBinary(USkeletalMesh* SkeletalMesh, const FString& BinaryPath, const FString& SourcePath)
{
	FWindowsBinWriter Writer(BinaryPath);
	if (!Writer.IsValid())
	{
		UE_LOG("SkeletalMesh binary save failed: could not open file. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		FAssetPackageHeader Header;
		Header.Type = static_cast<uint32>(EAssetPackageType::SkeletalMesh);
		Writer << Header;

		FAssetImportMetadata Metadata = MakeImportMetadata(SourcePath);
		Writer << Metadata;

		SkeletalMesh->Serialize(Writer);
	}
	catch (const std::exception&)
	{
		UE_LOG("SkeletalMesh binary save failed: serialization threw an exception. Path=%s", BinaryPath.c_str());
		return false;
	}

	return Writer.IsValid();
}

FString FMeshManager::GetStaticMeshBinaryFilePath(const FString& SourcePath)
{
	return GetMeshPackageFilePath(SourcePath, EAssetPackageType::StaticMesh);
}

FString FMeshManager::GetSkeletalMeshBinaryFilePath(const FString& SourcePath)
{
	return GetMeshPackageFilePath(SourcePath, EAssetPackageType::SkeletalMesh);
}

bool FMeshManager::IsAssetPackagePath(const FString& Path)
{
	return GetLowerExtension(Path) == MeshBinary::AssetPackageExtension;
}

bool FMeshManager::ReimportStaticMesh(const FString& BinaryPath, ID3D11Device* Device, UStaticMesh*& OutStaticMesh)
{
	OutStaticMesh = nullptr;

	FAssetImportMetadata Metadata;
	if (!LoadPackageMetadata(BinaryPath, EAssetPackageType::StaticMesh, Metadata)) return false;

	uint64 CurrentTimestamp = 0;
	uint64 CurrentFileSize = 0;
	if (!TryGetSourceFileState(Metadata.SourcePath, CurrentTimestamp, CurrentFileSize))
	{
		UE_LOG("StaticMesh reimport failed: source file is missing. Package=%s, Source=%s", BinaryPath.c_str(), Metadata.SourcePath.c_str());
		return false;
	}

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!ImportStaticMeshByExtension(Metadata.SourcePath, nullptr, *NewMeshAsset, ParsedMaterials))
	{
		return false;
	}

	const FString PackagePath = NormalizeProjectPath(BinaryPath);
	StaticMeshCache.erase(PackagePath);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = Metadata.SourcePath;
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	if (!SaveStaticMeshBinary(StaticMesh, PackagePath, Metadata.SourcePath)) return false;

	StaticMesh->InitResources(Device);
	StaticMesh->SetAssetPathFileName(PackagePath);
	StaticMeshCache[PackagePath] = StaticMesh;
	OutStaticMesh = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return true;
}

bool FMeshManager::ReimportSkeletalMesh(const FString& BinaryPath, ID3D11Device* Device, USkeletalMesh*& OutSkeletalMesh)
{
	OutSkeletalMesh = nullptr;

	FAssetImportMetadata Metadata;
	if (!LoadPackageMetadata(BinaryPath, EAssetPackageType::SkeletalMesh, Metadata)) return false;

	uint64 CurrentTimestamp = 0;
	uint64 CurrentFileSize = 0;
	if (!TryGetSourceFileState(Metadata.SourcePath, CurrentTimestamp, CurrentFileSize))
	{
		UE_LOG("SkeletalMesh reimport failed: source file is missing. Package=%s, Source=%s", BinaryPath.c_str(), Metadata.SourcePath.c_str());
		return false;
	}

	FSkeletalMesh* ImportedMesh = nullptr;
	if (!LoadSkeletalMeshAsset(Metadata.SourcePath, Device, ImportedMesh))
	{
		return false;
	}

	const FString PackagePath = NormalizeProjectPath(BinaryPath);
	SkeletalMeshCache.erase(PackagePath);

	USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	SkeletalMesh->SetSkeletalMaterials(std::move(FFbxImporter::SkeletalMaterials));
	SkeletalMesh->SetSkeletalMeshAsset(ImportedMesh);
	if (!SaveSkeletalMeshBinary(SkeletalMesh, PackagePath, Metadata.SourcePath)) return false;

	SkeletalMesh->InitResources(Device);
	SkeletalMesh->SetAssetPathFileName(PackagePath);
	SkeletalMeshCache[PackagePath] = SkeletalMesh;
	OutSkeletalMesh = SkeletalMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return true;
}

bool FMeshManager::IsStaticMeshPackage(const FString& Path)
{
	if (!IsAssetPackagePath(Path)) return false;

	FAssetImportMetadata Metadata;
	return LoadPackageMetadata(Path, EAssetPackageType::StaticMesh, Metadata);
}

bool FMeshManager::IsSkeletalMeshPackage(const FString& Path)
{
	if (!IsAssetPackagePath(Path)) return false;

	FAssetImportMetadata Metadata;
	return LoadPackageMetadata(Path, EAssetPackageType::SkeletalMesh, Metadata);
}

void FMeshManager::ScanMeshAssets()
{
	AvailableStaticMeshFiles.clear();
	AvailableSkeletalMeshFiles.clear();

	const std::filesystem::path MeshCacheRoot = FPaths::RootDir() + L"Content\\";
	if (!std::filesystem::exists(MeshCacheRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MeshCacheRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

		if (Ext != MeshBinary::StaticMeshBinaryExtension) continue;	

		// MeshCache 목록은 새 확장자만 보여준다.
		// Static은 .statbin, Skeletal은 .sketbin으로 분리해서 수집한다.
		TArray<FMeshAssetListItem>* TargetList = nullptr;

		FString RelPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());

		FAssetImportMetadata Metadata;
		if (LoadPackageMetadata(RelPath, EAssetPackageType::StaticMesh, Metadata))
		{
			TargetList = &AvailableStaticMeshFiles;
		}
		else if (LoadPackageMetadata(RelPath, EAssetPackageType::SkeletalMesh, Metadata))
		{
			TargetList = &AvailableSkeletalMeshFiles;
		}
		else
		{
			continue;
		}

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = RelPath;
		TargetList->push_back(std::move(Item));
	}
}

void FMeshManager::ScanMeshSourceFiles()
{
	AvailableStaticMeshSourceFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Data/";

	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".obj" && Ext != L".fbx") continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableStaticMeshSourceFiles.push_back(std::move(Item));
	}
}

UStaticMesh* FMeshManager::LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	const bool bInputIsPackage = IsAssetPackagePath(PathFileName);
	if (bInputIsPackage)
	{
		UE_LOG("StaticMesh load failed: StaticMesh binary cannot be loaded as StaticMesh. Path=%s", PathFileName.c_str());
		return nullptr;
	}
	if (!IsSupportedStaticMeshSourcePath(PathFileName))
	{
		UE_LOG("StaticMesh import failed: option import only supports source .obj/.fbx paths. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const FString CacheKey = GetStaticMeshBinaryFilePath(PathFileName);

	// import 옵션이 바뀌면 같은 원본도 다른 Mesh가 될 수 있다.
	// 그래서 기존 캐시를 지우고 새 .statbin을 만든다.
	StaticMeshCache.erase(CacheKey);

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!ImportStaticMeshByExtension(PathFileName, &Options, *NewMeshAsset, ParsedMaterials))
	{
		UE_LOG("StaticMesh import failed: empty mesh will not be added to cache. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = NormalizeProjectPath(PathFileName);
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	// import가 끝난 StaticMesh는 .statbin으로 저장한다.
	// 다음 로드부터는 무거운 원본 파싱을 건너뛸 수 있다.
	SaveStaticMeshBinary(StaticMesh, CacheKey, PathFileName);

	StaticMesh->InitResources(InDevice);
	StaticMesh->SetAssetPathFileName(CacheKey);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FMeshManager::LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const bool bInputIsPackage = IsAssetPackagePath(PathFileName);
	if (!bInputIsPackage && !IsSupportedStaticMeshSourcePath(PathFileName))
	{
		UE_LOG("StaticMesh load failed: unsupported path. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const FString CacheKey = GetStaticMeshBinaryFilePath(PathFileName);
	
	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	const std::filesystem::path BinaryPath(FPaths::ToWide(CacheKey));
	if (std::filesystem::exists(BinaryPath))
	{
		UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
		if (LoadStaticMeshBinary(StaticMesh, CacheKey))
		{
			bool bMissingSource = false;
			if (IsPackageSourceStale(CacheKey, EAssetPackageType::StaticMesh, bMissingSource))
			{
				UE_LOG("StaticMesh package is stale. Package=%s MissingSource=%s", CacheKey.c_str(), bMissingSource ? "true" : "false");
			}

			StaticMesh->InitResources(InDevice);
			StaticMesh->SetAssetPathFileName(CacheKey);
			StaticMeshCache[CacheKey] = StaticMesh;
			return StaticMesh;
		}

		if (bInputIsPackage)
		{
			// Binary 경로만 받으면 원본 위치를 확실히 알 수 없다.
			// 이 경우에는 새 import를 시도하지 않고 실패로 끝낸다.
			return nullptr;
		}

		UE_LOG("StaticMesh binary load failed: source path is available, reimporting. Source=%s Binary=%s", PathFileName.c_str(), CacheKey.c_str());
	}
	else if (bInputIsPackage)
	{
		UE_LOG("StaticMesh load failed: StaticMesh binary file does not exist. Path=%s", CacheKey.c_str());
		return nullptr;
	}

	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();
	TArray<FStaticMaterial> ParsedMaterials;
	if (!ImportStaticMeshByExtension(PathFileName, nullptr, *NewMeshAsset, ParsedMaterials))
	{
		UE_LOG("StaticMesh import failed: empty mesh will not be added to cache. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	NewMeshAsset->PathFileName = NormalizeProjectPath(PathFileName);
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	// .statbin이 없을 때만 원본 파일을 import한다.
	// import가 성공하면 바로 캐시 파일을 만들어 둔다.
	SaveStaticMeshBinary(StaticMesh, CacheKey, PathFileName);

	StaticMesh->InitResources(InDevice);
	StaticMesh->SetAssetPathFileName(CacheKey);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

void FMeshManager::ScanFbxSourceFiles()
{
	AvailableFbxSourceFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Data/";

	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".fbx") continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableFbxSourceFiles.push_back(std::move(Item));
	}
}

void FMeshManager::ReleaseAllGPU()
{
	// Static Mesh
	for (auto& [Key, Mesh] : StaticMeshCache)
	{
		if (Mesh)
		{
			FStaticMesh* Asset = Mesh->GetStaticMeshAsset();
			if (Asset && Asset->RenderBuffer)
			{
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
			// LOD 버퍼도 해제
			for (uint32 LOD = 1; LOD < UStaticMesh::MAX_LOD_COUNT; ++LOD)
			{
				FMeshBuffer* LODBuffer = Mesh->GetLODMeshBuffer(LOD);
				if (LODBuffer)
				{
					LODBuffer->Release();
				}
			}
		}
	}
	StaticMeshCache.clear();

	// Skeletal Mesh
	for (auto& [Key, Mesh] : SkeletalMeshCache)
	{
		if (Mesh)
		{
			FSkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
			if (Asset && Asset->RenderBuffer)
			{
				Asset->RenderBuffer->Release();
				Asset->RenderBuffer.reset();
			}
		}
	}
	SkeletalMeshCache.clear();
}

USkeletalMesh* FMeshManager::LoadSkeletalMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const std::wstring Ext = GetLowerExtension(PathFileName);

	const bool bInputIsPackage = IsAssetPackagePath(PathFileName);
	if (!bInputIsPackage && !IsSupportedSkeletalMeshSourcePath(PathFileName))
	{
		UE_LOG("SkeletalMesh load failed: unsupported path. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const FString CacheKey = GetSkeletalMeshBinaryFilePath(PathFileName);
	auto It = SkeletalMeshCache.find(CacheKey);
	if (It != SkeletalMeshCache.end())
	{
		return It->second;
	}

	const std::filesystem::path BinaryPath(FPaths::ToWide(CacheKey));
	if (std::filesystem::exists(BinaryPath))
	{
		USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
		if (LoadSkeletalMeshBinary(SkeletalMesh, CacheKey))
		{
			bool bMissingSource = false;
			if (IsPackageSourceStale(CacheKey, EAssetPackageType::SkeletalMesh, bMissingSource))
			{
				UE_LOG("SkeletalMesh package is stale. Package=%s MissingSource=%s", CacheKey.c_str(), bMissingSource ? "true" : "false");
			}

			SkeletalMesh->InitResources(InDevice);
			SkeletalMesh->SetAssetPathFileName(CacheKey);
			SkeletalMeshCache[CacheKey] = SkeletalMesh;
			return SkeletalMesh;
		}

		if (bInputIsPackage)
		{
			return nullptr;
		}

		UE_LOG("SkeletalMesh binary load failed: source path is available, reimporting. Source=%s Binary=%s", PathFileName.c_str(), CacheKey.c_str());
	}
	else if (bInputIsPackage)
	{
		UE_LOG("SkeletalMesh load failed: SkeletalMesh binary file does not exist. Path=%s", CacheKey.c_str());
		return nullptr;
	}

	FSkeletalMesh* ImportedMesh = nullptr;
	if (!LoadSkeletalMeshAsset(PathFileName, InDevice, ImportedMesh))
	{
		UE_LOG("SkeletalMesh import failed: empty mesh will not be added to cache. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
	SkeletalMesh->SetSkeletalMaterials(std::move(FFbxImporter::SkeletalMaterials));
	SkeletalMesh->SetSkeletalMeshAsset(ImportedMesh);

	// SkeletalMesh는 Bone, Section, Vertex/Index까지 함께 저장한다.
	// StaticMesh와 섞이지 않도록 .sketbin으로 따로 캐시한다.
	SaveSkeletalMeshBinary(SkeletalMesh, CacheKey, PathFileName);

	SkeletalMesh->InitResources(InDevice);
	SkeletalMesh->SetAssetPathFileName(CacheKey);
	SkeletalMeshCache[CacheKey] = SkeletalMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return SkeletalMesh;
}

bool FMeshManager::LoadSkeletalMeshAsset(const FString& PathFileName, ID3D11Device* InDevice, FSkeletalMesh*& OutMesh)
{
	(void)InDevice;

	if (!IsSupportedSkeletalMeshSourcePath(PathFileName))
	{
		UE_LOG("SkeletalMesh import failed: only source FBX paths can be imported. Path=%s", PathFileName.c_str());
		return false;
	}

	if (!FFbxImporter::Import(PathFileName))
	{
		return false;
	}

	std::unique_ptr<FSkeletalMesh> NewMesh = std::make_unique<FSkeletalMesh>();
	NewMesh->PathFileName = NormalizeProjectPath(PathFileName);
	NewMesh->Vertices = std::move(FFbxImporter::Vertices);
	NewMesh->Indices = std::move(FFbxImporter::Indices);
	NewMesh->Sections = std::move(FFbxImporter::Sections);
	NewMesh->MeshRanges = std::move(FFbxImporter::MeshRanges);
	NewMesh->Bones = std::move(FFbxImporter::Bones);

	OutMesh = NewMesh.release();
	return true;
}
