#include "MeshManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/MeshBinaryHeader.h"
#include "Materials/Material.h"
#include "Core/Log.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include <filesystem>
#include <algorithm>
#include <cwctype>

TMap<FString, UStaticMesh*> FMeshManager::StaticMeshCache;
TMap<FString, USkeletalMesh*> FMeshManager::SkeletalMeshCache;
TArray<FMeshAssetListItem> FMeshManager::AvailableStaticMeshFiles;
TArray<FMeshAssetListItem> FMeshManager::AvailableStaticMeshSourceFiles;
TArray<FMeshAssetListItem> FMeshManager::AvailableSkeletalMeshFiles;

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

	UE_LOG("Unsupported static mesh format: %s", PathFileName.c_str());
	return false;
}

FString FMeshManager::GetBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();
	// 비교전에 확장자를 모두 소문자로 변경한다(Bin, bIn, biN등등 대비용)
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	// 이미 bin 경로가 들어온 경우에는 그대로 사용
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	EnsureMeshCacheDirExists();

	// 상대 경로로 반환
	std::filesystem::path RelPath = std::filesystem::path(L"Asset\\MeshCache") / SrcPath.stem();
	RelPath += L".bin";

	return FPaths::ToUtf8(RelPath.generic_wstring());
}


void FMeshManager::ScanMeshAssets()
{
	AvailableStaticMeshFiles.clear();

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
		if (Path.extension() != L".bin") continue;

		FMeshAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableStaticMeshFiles.push_back(std::move(Item));
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

		// 대소문자 무시
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
	FString CacheKey = GetBinaryFilePath(PathFileName);

	// 옵션이 다를 수 있으므로 기존 캐시 무효화
	StaticMeshCache.erase(CacheKey);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;

	// 항상 리빌드 (옵션이 달라질 수 있음)
	FStaticMesh* NewMeshAsset = new FStaticMesh();
	TArray<FStaticMaterial> ParsedMaterials;

	if (ImportStaticMeshByExtension(PathFileName, &Options, *NewMeshAsset, ParsedMaterials))
	{
		NewMeshAsset->PathFileName = PathFileName;
		// MaterialIndex 캐싱을 위해 Materials를 먼저 설정

		StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
		StaticMesh->SetStaticMeshAsset(NewMeshAsset);

		// .bin 저장 (메시 지오메트리 + Material JSON 경로 참조)
		FWindowsBinWriter Writer(BinPath);
		if (Writer.IsValid())
		{
			// StaticMesh임을 나타내는 Header 추가(Magic Number)
			MeshBinary::WriteHeader(Writer, MeshBinary::StaticMeshMagic);
			StaticMesh->Serialize(Writer);
		}
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	// 리프레시
	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FMeshManager::LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	FString CacheKey = GetBinaryFilePath(PathFileName);

	// BinPath 기반 캐시 확인
	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	// UStaticMesh 생성 + FStaticMesh 소유권 이전 + 머티리얼 설정
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;
	bool bNeedRebuild = true;

	// 3. 타임스탬프 비교 (디스크 캐시 확인)
	std::filesystem::path BinPathW(FPaths::ToWide(BinPath));
	std::filesystem::path PathFileNameW(FPaths::ToWide(PathFileName));
	if (std::filesystem::exists(BinPathW))
	{
		if (!std::filesystem::exists(PathFileNameW) || PathFileName == BinPath ||
			std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(PathFileNameW))
		{
			bNeedRebuild = false;
		}
	}

	if (!bNeedRebuild)
	{
		// BIN 파일에서 통째로 로드 (Material은 JSON 경로로 FMaterialManager를 통해 복원)
		FWindowsBinReader Reader(BinPath);
		if (Reader.IsValid())
		{
			// Magic Number 정보 기반으로 Static Mesh로 Serialize할지 말지 체크(MeshBinaryHeader.h 참고)
			if (MeshBinary::ReadHeader(Reader, MeshBinary::StaticMeshMagic))
			{
				StaticMesh->Serialize(Reader);
			}
			else
			{
				UE_LOG("Invalid StaticMesh binary header: %s", BinPath.c_str());
				bNeedRebuild = true;
			}
		}
		else
		{
			bNeedRebuild = true; // 읽기 실패 시 강제 파싱
		}
	}

	if (bNeedRebuild)
	{
		// 원본 OBJ 경로 결정 — .bin에서 로드한 경우 내부에 저장된 원본 경로 사용
		FString SourcePath = PathFileName;
		if (StaticMesh->GetStaticMeshAsset() && !StaticMesh->GetStaticMeshAsset()->PathFileName.empty())
			SourcePath = StaticMesh->GetStaticMeshAsset()->PathFileName;

		// 무거운 source mesh 파싱 진행
		FStaticMesh* NewMeshAsset = new FStaticMesh();
		TArray<FStaticMaterial> ParsedMaterials;

		if (ImportStaticMeshByExtension(SourcePath, nullptr, *NewMeshAsset, ParsedMaterials))
		{
			NewMeshAsset->PathFileName = SourcePath;
			// MaterialIndex 캐싱을 위해 Materials를 먼저 설정
			StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
			StaticMesh->SetStaticMeshAsset(NewMeshAsset);

			// 파싱 결과를 하드디스크에 굽기 (다음 로딩 속도 최적화)
			FWindowsBinWriter Writer(BinPath);
			if (Writer.IsValid())
			{
				// binary 파일 쓰기 작업시 Magic Number 추가
				MeshBinary::WriteHeader(Writer, MeshBinary::StaticMeshMagic);
				StaticMesh->Serialize(Writer);
			}
		}
	}

	StaticMesh->InitResources(InDevice);

	// 캐시 등록
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();

	return StaticMesh;
}
void FMeshManager::ScanFbxSourceFiles()
{
	AvailableSkeletalMeshFiles.clear();

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
		AvailableSkeletalMeshFiles.push_back(std::move(Item));
	}
}

void FMeshManager::ReleaseAllGPU()
{
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
}

USkeletalMesh* FMeshManager::LoadSkeletalMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	if (SkeletalMeshCache.find(PathFileName) != SkeletalMeshCache.end())
	{
		return SkeletalMeshCache[PathFileName];
	}

	FSkeletalMesh* SkeletalMeshAsset = nullptr;
	if (!LoadSkeletalMeshAsset(PathFileName, InDevice, SkeletalMeshAsset))
	{
		return nullptr;
	}
	USkeletalMesh* SkeletalMesh = new USkeletalMesh();
	SkeletalMesh->SetSkeletalMaterials(std::move(FFbxImporter::SkeletalMaterials));
	SkeletalMesh->SetSkeletalMeshAsset(SkeletalMeshAsset);

	SkeletalMesh->InitResources(InDevice);

	SkeletalMeshCache[PathFileName] = SkeletalMesh;

	return SkeletalMesh;
}

bool FMeshManager::LoadSkeletalMeshAsset(const FString& PathFileName, ID3D11Device* InDevice, FSkeletalMesh*& OutMesh)
{
	if (!FFbxImporter::Import(PathFileName))
	{
		//UE_LOG("Failed to import FBX file: %s", FilePath.c_str());
		return false;
	}

	OutMesh = new FSkeletalMesh();
	OutMesh->PathFileName = PathFileName;
	OutMesh->Vertices = std::move(FFbxImporter::Vertices);
	OutMesh->Indices = std::move(FFbxImporter::Indices);
	OutMesh->Sections = std::move(FFbxImporter::Sections);
	OutMesh->MeshRanges = std::move(FFbxImporter::MeshRanges);
	OutMesh->Bones = std::move(FFbxImporter::Bones);

	return true;
}
