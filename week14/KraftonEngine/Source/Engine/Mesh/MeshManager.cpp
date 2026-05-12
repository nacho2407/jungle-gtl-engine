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
		std::wstring CacheDir = FPaths::RootDir() + L"Asset\\MeshCache\\";
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

static FString GetMeshBinaryFilePath(const FString& SourcePath, const wchar_t* Extension)
{
	std::filesystem::path SrcPath(FPaths::ToWide(SourcePath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	if (Ext == Extension)
	{
		return NormalizeProjectPath(SourcePath);
	}

	EnsureMeshCacheDirExists();

	// 경로는 동일하지만, 확장자를 나눠서 Mesh 타입을 알 수 있게 한다.
	std::filesystem::path RelPath = std::filesystem::path(L"Asset\\MeshCache") / SrcPath.stem();
	RelPath += Extension;
	return FPaths::ToUtf8(RelPath.generic_wstring());
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
		// .statbin은 StaticMesh 전용 파일이다.
		// 확장자로 이미 타입을 알 수 있으므로 바로 StaticMesh 데이터로 읽는다.
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

static bool SaveStaticMeshBinary(UStaticMesh* StaticMesh, const FString& BinaryPath)
{
	FWindowsBinWriter Writer(BinaryPath);
	if (!Writer.IsValid())
	{
		UE_LOG("StaticMesh binary save failed: could not open file. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		// .statbin에는 UStaticMesh 데이터만 저장한다.
		// 타입 표시는 파일 확장자가 맡는다.
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
		// .sketbin은 SkeletalMesh 전용 파일이다.
		// Bone, SkinWeight, Section 정보를 포함하므로 StaticMesh와 섞어 읽지 않는다.
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

static bool SaveSkeletalMeshBinary(USkeletalMesh* SkeletalMesh, const FString& BinaryPath)
{
	FWindowsBinWriter Writer(BinaryPath);
	if (!Writer.IsValid())
	{
		UE_LOG("SkeletalMesh binary save failed: could not open file. Path=%s", BinaryPath.c_str());
		return false;
	}

	try
	{
		// .sketbin에는 USkeletalMesh 데이터만 저장한다.
		// 타입 표시는 파일 확장자가 맡는다.
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
	return GetMeshBinaryFilePath(SourcePath, MeshBinary::StaticMeshBinaryExtension);
}

FString FMeshManager::GetSkeletalMeshBinaryFilePath(const FString& SourcePath)
{
	return GetMeshBinaryFilePath(SourcePath, MeshBinary::SkeletalMeshBinaryExtension);
}

bool FMeshManager::IsStaticMeshBinaryPath(const FString& Path)
{
	return GetLowerExtension(Path) == MeshBinary::StaticMeshBinaryExtension;
}

bool FMeshManager::IsSkeletalMeshBinaryPath(const FString& Path)
{
	return GetLowerExtension(Path) == MeshBinary::SkeletalMeshBinaryExtension;
}

void FMeshManager::ScanMeshAssets()
{
	AvailableStaticMeshFiles.clear();
	AvailableSkeletalMeshFiles.clear();

	const std::filesystem::path MeshCacheRoot = FPaths::RootDir() + L"Asset\\MeshCache\\";
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

		// MeshCache 목록은 새 확장자만 보여준다.
		// Static은 .statbin, Skeletal은 .sketbin으로 분리해서 수집한다.
		TArray<FMeshAssetListItem>* TargetList = nullptr;
		if (Ext == MeshBinary::StaticMeshBinaryExtension)
		{
			TargetList = &AvailableStaticMeshFiles;
		}
		else if (Ext == MeshBinary::SkeletalMeshBinaryExtension)
		{
			TargetList = &AvailableSkeletalMeshFiles;
		}
		else
		{
			continue;
		}

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		TargetList->push_back(std::move(Item));
	}
}

void FMeshManager::ScanMeshSourceFiles()
{
	AvailableStaticMeshSourceFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Data\\";

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
	const std::wstring Ext = GetLowerExtension(PathFileName);
	if (Ext == MeshBinary::SkeletalMeshBinaryExtension)
	{
		UE_LOG("StaticMesh load failed: SkeletalMesh binary(.sketbin) cannot be loaded as StaticMesh. Path=%s", PathFileName.c_str());
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
	SaveStaticMeshBinary(StaticMesh, CacheKey);

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FMeshManager::LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const std::wstring Ext = GetLowerExtension(PathFileName);
	if (Ext == MeshBinary::SkeletalMeshBinaryExtension)
	{
		UE_LOG("StaticMesh load failed: SkeletalMesh binary(.sketbin) cannot be loaded as StaticMesh. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const bool bInputIsBinary = Ext == MeshBinary::StaticMeshBinaryExtension;
	if (!bInputIsBinary && !IsSupportedStaticMeshSourcePath(PathFileName))
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
			StaticMesh->InitResources(InDevice);
			StaticMeshCache[CacheKey] = StaticMesh;
			return StaticMesh;
		}

		if (bInputIsBinary)
		{
			// Binary 경로만 받으면 원본 위치를 확실히 알 수 없다.
			// 이 경우에는 새 import를 시도하지 않고 실패로 끝낸다.
			return nullptr;
		}

		UE_LOG("StaticMesh binary load failed: source path is available, reimporting. Source=%s Binary=%s", PathFileName.c_str(), CacheKey.c_str());
	}
	else if (bInputIsBinary)
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
	SaveStaticMeshBinary(StaticMesh, CacheKey);

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

void FMeshManager::ScanFbxSourceFiles()
{
	AvailableFbxSourceFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Data\\";

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
	if (Ext == MeshBinary::StaticMeshBinaryExtension)
	{
		UE_LOG("SkeletalMesh load failed: StaticMesh binary(.statbin) cannot be loaded as SkeletalMesh. Path=%s", PathFileName.c_str());
		return nullptr;
	}

	const bool bInputIsBinary = Ext == MeshBinary::SkeletalMeshBinaryExtension;
	if (!bInputIsBinary && !IsSupportedSkeletalMeshSourcePath(PathFileName))
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
			SkeletalMesh->InitResources(InDevice);
			SkeletalMeshCache[CacheKey] = SkeletalMesh;
			return SkeletalMesh;
		}

		if (bInputIsBinary)
		{
			return nullptr;
		}

		UE_LOG("SkeletalMesh binary load failed: source path is available, reimporting. Source=%s Binary=%s", PathFileName.c_str(), CacheKey.c_str());
	}
	else if (bInputIsBinary)
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
	SaveSkeletalMeshBinary(SkeletalMesh, CacheKey);

	SkeletalMesh->InitResources(InDevice);
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
