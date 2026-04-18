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
#include <sstream>
#include <iomanip>
#include <array>
#include <cwctype>

// 로드된 UStaticMesh를 바이너리 캐시 경로 기준으로 보관하는 런타임 캐시
TMap<FString, UStaticMesh*> FObjManager::StaticMeshCache;

// 에디터/UI에서 보여줄 수 있는 .bin 메쉬 캐시 목록
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;

// 에디터/UI에서 보여줄 수 있는 원본 .obj 목록
TArray<FMeshAssetListItem> FObjManager::AvailableObjFiles;

static void EnsureMeshCacheDirExists()
{
	// MeshCache 디렉토리는 한 번만 생성하면 되므로 static 플래그 사용
	static bool bCreated = false;
	if (!bCreated)
	{
		std::wstring CacheDir = FPaths::RootDir() + L"Asset\\MeshCache\\";
		FPaths::CreateDir(CacheDir);
		bCreated = true;
	}
}

static uint64 HashNormalizedPath(const FString& OriginalPath)
{
	// 입력 경로를 정규화해서
	// 같은 파일을 가리키는 경로 표기 차이(../, 대소문자 등)로 해시가 달라지는 것을 최소화
	std::filesystem::path Path = std::filesystem::path(FPaths::ToWide(OriginalPath)).lexically_normal();
	std::error_code Ec;
	std::filesystem::path Canonical = std::filesystem::weakly_canonical(Path, Ec);
	const std::wstring Normalized = Ec ? Path.generic_wstring() : Canonical.generic_wstring();

	// FNV-1a 64bit 해시
	// 경로 문자열을 소문자 기준으로 해싱해서 Windows 환경의 대소문자 차이를 완화
	uint64 Hash = 1469598103934665603ull;
	for (wchar_t Ch : Normalized)
	{
		Hash ^= static_cast<uint64>(std::towlower(Ch));
		Hash *= 1099511628211ull;
	}
	return Hash;
}

static FString BuildHashedMeshCachePath(const FString& OriginalPath)
{
	// 캐시 폴더가 없으면 먼저 생성
	EnsureMeshCacheDirExists();

	// 원본 경로를 해시해서 충돌 가능성은 낮추고,
	// 캐시 파일명은 항상 일정한 규칙으로 생성
	std::ostringstream Oss;
	Oss << "Asset/MeshCache/" << std::hex << std::setw(16) << std::setfill('0') << HashNormalizedPath(OriginalPath) << ".bin";
	return Oss.str();
}

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);

	// 이미 .bin 파일이면 그대로 사용
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	// 원본(.obj 등)이면 대응되는 해시 기반 캐시 경로 생성
	return BuildHashedMeshCachePath(OriginalPath);
}

void FObjManager::ScanMeshAssets()
{
	// UI 표시용 목록을 다시 스캔하므로 기존 목록 초기화
	AvailableMeshFiles.clear();

	const std::filesystem::path MeshCacheRoot = FPaths::RootDir() + L"Asset\\MeshCache\\";

	// 캐시 폴더 자체가 없으면 스캔할 것이 없음
	if (!std::filesystem::exists(MeshCacheRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	// MeshCache 아래의 모든 .bin 파일을 재귀적으로 수집
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(MeshCacheRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		if (Path.extension() != L".bin") continue;

		FMeshAssetListItem Item;
		// 화면 표시용 이름: 파일명(확장자 제외)
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		// 내부 경로: 프로젝트 루트 기준 상대 경로
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMeshFiles.push_back(std::move(Item));
	}
}

void FObjManager::ScanObjSourceFiles()
{
	// UI 표시용 OBJ 원본 목록 초기화
	AvailableObjFiles.clear();

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Data\\";

	// Data 폴더가 없으면 원본 obj도 없음
	if (!std::filesystem::exists(DataRoot))
	{
		return;
	}

	const std::filesystem::path ProjectRoot(FPaths::RootDir());

	// Data 아래의 모든 .obj 파일을 재귀적으로 수집
	for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
	{
		if (!Entry.is_regular_file()) continue;

		const std::filesystem::path& Path = Entry.path();
		std::wstring Ext = Path.extension().wstring();

		// 확장자 비교는 대소문자 무시
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
		if (Ext != L".obj") continue;

		FMeshAssetListItem Item;
		// 표시용 이름은 파일명 전체
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
	// 결과를 써넣을 UStaticMesh가 없으면 실패
	if (!StaticMesh)
	{
		return false;
	}

	// importer가 채워 넣을 실제 메쉬 에셋 데이터
	std::unique_ptr<FStaticMesh> NewMeshAsset = std::make_unique<FStaticMesh>();

	// importer가 파싱한 material 슬롯 정보
	TArray<FStaticMaterial> ParsedMaterials;

	// 옵션이 있으면 옵션 포함 import, 없으면 기본 import
	const bool bImported = Options
		? FObjImporter::Import(ObjPath, *Options, *NewMeshAsset, ParsedMaterials)
		: FObjImporter::Import(ObjPath, *NewMeshAsset, ParsedMaterials);

	if (!bImported)
	{
		UE_LOG("[ERROR] Failed to import static mesh source: %s", ObjPath.c_str());
		return false;
	}

	// import 성공 시, 원본 경로를 기록하고
	// UStaticMesh에 재료/메쉬 본체를 넘겨줌
	NewMeshAsset->PathFileName = ObjPath;
	StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
	StaticMesh->SetStaticMeshAsset(NewMeshAsset.release());

	// 다음 로드에서 재사용할 수 있도록 바이너리 캐시에 직렬화 저장
	FWindowsBinWriter Writer(BinPath);
	if (Writer.IsValid())
	{
		StaticMesh->Serialize(Writer);
	}
	else
	{
		// 캐시 저장 실패는 경고만 남기고, import 자체는 성공으로 처리
		UE_LOG("[WARN] Failed to open mesh cache for write: %s", BinPath.c_str());
	}

	return true;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	// 옵션을 명시한 로드는 "강제 재임포트" 성격으로 동작
	// 기존 런타임 캐시를 지우고 새로 만든다
	const FString CacheKey = GetBinaryFilePath(PathFileName);
	StaticMeshCache.erase(CacheKey);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	if (!TryImportStaticMesh(PathFileName, &Options, StaticMesh, CacheKey))
	{
		return nullptr;
	}

	// CPU 데이터 import 후 GPU 리소스 생성
	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	// 에디터에서 보이는 asset/material 목록도 갱신
	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();
	return StaticMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	const FString CacheKey = GetBinaryFilePath(PathFileName);

	// 이미 같은 캐시 키로 로드된 메쉬가 있으면 바로 반환
	auto It = StaticMeshCache.find(CacheKey);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	const FString BinPath = CacheKey;
	bool bNeedRebuild = true;

	std::filesystem::path BinPathW(FPaths::ToWide(BinPath));
	std::filesystem::path PathFileNameW(FPaths::ToWide(PathFileName));

	// 캐시 파일이 존재하면 재빌드가 꼭 필요한지 검사
	if (std::filesystem::exists(BinPathW))
	{
		// 아래 경우엔 캐시를 그대로 사용:
		// 1) 원본 파일이 더 이상 존재하지 않음
		// 2) 입력 자체가 .bin 파일임
		// 3) 캐시 파일이 원본보다 최신이거나 동일함
		if (!std::filesystem::exists(PathFileNameW) || PathFileName == BinPath ||
			std::filesystem::last_write_time(BinPathW) >= std::filesystem::last_write_time(PathFileNameW))
		{
			bNeedRebuild = false;
		}
	}

	if (!bNeedRebuild)
	{
		// 캐시에서 바로 deserialize
		FWindowsBinReader Reader(BinPath);
		if (Reader.IsValid())
		{
			StaticMesh->Serialize(Reader);
		}
		else
		{
			// 읽기 실패 시 import 경로로 fallback
			UE_LOG("[WARN] Failed to open mesh cache for read: %s", BinPath.c_str());
			bNeedRebuild = true;
		}
	}

	if (bNeedRebuild)
	{
		// 캐시가 없거나 오래됐거나 읽기 실패 시 원본에서 다시 import
		if (!TryImportStaticMesh(PathFileName, nullptr, StaticMesh, BinPath))
		{
			return nullptr;
		}
	}

	// import/deserialize 후에도 실제 에셋 본체가 비어 있으면 오류
	if (!StaticMesh->GetStaticMeshAsset())
	{
		UE_LOG("[ERROR] Static mesh asset was empty after load: %s", PathFileName.c_str());
		return nullptr;
	}

	// GPU 버퍼 생성 후 런타임 캐시에 등록
	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	// 에디터 표시용 목록 갱신
	ScanMeshAssets();
	FMaterialManager::Get().ScanMaterialAssets();
	return StaticMesh;
}

void FObjManager::ReleaseAllGPU()
{
	// 현재 캐시에 들어 있는 모든 메쉬의 GPU 리소스를 해제
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

			// LOD 버퍼도 별도로 해제
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

	// GPU 리소스를 모두 내렸으므로 런타임 캐시도 비움
	StaticMeshCache.clear();
}