#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Materials/Material.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include <filesystem>
#include <algorithm>

TMap<FString, UStaticMesh*> FObjManager::StaticMeshCache;
TMap<FString, UMaterial*> FObjManager::MaterialCache;
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;
TArray<FMeshAssetListItem> FObjManager::AvailableObjFiles;
TArray<FMaterialAssetListItem> FObjManager::AvailableMaterialFiles;

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

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();

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

FString FObjManager::GetMBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();

	// 이미 bin 경로가 들어온 경우에는 그대로 사용
	if (Ext == L".mbin")
	{
		return OriginalPath;
	}

	EnsureMeshCacheDirExists();

	// 상대 경로로 반환 — filename()을 사용해 블렌더 ".001" 접미사가 잘리지 않도록 함
	std::filesystem::path RelPath = std::filesystem::path(L"Asset\\MeshCache") / SrcPath.filename();
	RelPath += L".mbin";

	return FPaths::ToUtf8(RelPath.generic_wstring());
}

void FObjManager::ScanMeshAssets()
{
	AvailableMeshFiles.clear();

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
		AvailableMeshFiles.push_back(std::move(Item));
	}
}

void FObjManager::ScanMaterialAssets()
{
	AvailableMaterialFiles.clear();

	// .mbin 파일도 .bin과 동일하게 MeshCache 폴더에 생성됨
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

		// 확장자가 .mbin인지 확인
		if (Path.extension() != L".mbin") continue;
		if (Path.stem() == L"None") continue; // Fallback 머티리얼은 목록에서 제외

		FMaterialAssetListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.stem().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableMaterialFiles.push_back(std::move(Item));
	}
}

void FObjManager::ScanObjSourceFiles()
{
	AvailableObjFiles.clear();

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
		if (Ext != L".obj") continue;

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

const TArray<FMaterialAssetListItem>& FObjManager::GetAvailableMaterialFiles()
{
	return AvailableMaterialFiles;
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableObjFiles()
{
	return AvailableObjFiles;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	FString CacheKey = GetBinaryFilePath(PathFileName);

	// 옵션이 다를 수 있으므로 기존 캐시 무효화
	StaticMeshCache.erase(CacheKey);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;

	// 항상 리빌드 (옵션이 달라질 수 있음)
	FStaticMesh* NewMeshAsset = new FStaticMesh();
	TArray<FStaticMaterial> ParsedMaterials;

	if (FObjImporter::Import(PathFileName, Options, *NewMeshAsset, ParsedMaterials))
	{
		// 머티리얼 .mbin 저장 ("None" Fallback 머티리얼은 저장하지 않음)
		for (auto& Mat : ParsedMaterials)
		{
			if (Mat.MaterialInterface)
			{
				MaterialCache[Mat.MaterialInterface->PathFileName] = Mat.MaterialInterface;
				if (Mat.MaterialInterface->PathFileName == "None")
					continue;

				FString MatBinPath = FObjManager::GetMBinaryFilePath(Mat.MaterialInterface->PathFileName);

				FWindowsBinWriter MatWriter(MatBinPath);
				if (MatWriter.IsValid())
				{
					Mat.MaterialInterface->Serialize(MatWriter);
				}
			}
		}

		NewMeshAsset->PathFileName = PathFileName;
		// MaterialIndex 캐싱을 위해 Materials를 먼저 설정
		StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
		StaticMesh->SetStaticMeshAsset(NewMeshAsset);

		// .bin 저장
		FWindowsBinWriter Writer(BinPath);
		if (Writer.IsValid())
		{
			StaticMesh->Serialize(Writer);
		}
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[CacheKey] = StaticMesh;

	// 리프레시
	ScanMeshAssets();
	ScanMaterialAssets();

	return StaticMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
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
		// BIN 파일에서 통째로 로드
		FWindowsBinReader Reader(BinPath);
		if (Reader.IsValid())
		{
			StaticMesh->Serialize(Reader);

			// .mbin 누락 등으로 머티리얼이 비어있으면 OBJ 재파싱으로 전환
			for (const auto& Mat : StaticMesh->GetStaticMaterials())
			{
				if (Mat.MaterialInterface && Mat.MaterialInterface->PathFileName.empty())
				{
					bNeedRebuild = true;
					break;
				}
			}

			// 인라인 복구된 머티리얼의 .mbin 저장 + PathFileName 키로 캐시 등록
			if (!bNeedRebuild)
			{
				for (const auto& Mat : StaticMesh->GetStaticMaterials())
				{
					if (Mat.MaterialInterface && !Mat.MaterialInterface->PathFileName.empty()
						&& Mat.MaterialInterface->PathFileName != "None")
					{
						// PostEditProperty 등에서 원본 경로로 조회 시 캐시 히트되도록 등록
						MaterialCache[Mat.MaterialInterface->PathFileName] = Mat.MaterialInterface;

						// .mbin이 없으면 디스크에 저장 (다음 세션용)
						FString MatBinPath = GetMBinaryFilePath(Mat.MaterialInterface->PathFileName);
						std::filesystem::path MBinPathW(FPaths::ToWide(MatBinPath));
						if (!std::filesystem::exists(MBinPathW))
						{
							FWindowsBinWriter MatWriter(MatBinPath);
							if (MatWriter.IsValid())
							{
								Mat.MaterialInterface->Serialize(MatWriter);
							}
						}
					}
				}
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
		FString ObjPath = PathFileName;
		if (StaticMesh->GetStaticMeshAsset() && !StaticMesh->GetStaticMeshAsset()->PathFileName.empty())
			ObjPath = StaticMesh->GetStaticMeshAsset()->PathFileName;

		// 무거운 OBJ 파싱 진행
		FStaticMesh* NewMeshAsset = new FStaticMesh();
		TArray<FStaticMaterial> ParsedMaterials;

		if (FObjImporter::Import(ObjPath, *NewMeshAsset, ParsedMaterials))
		{
			for (auto& Mat : ParsedMaterials)
			{
				if (Mat.MaterialInterface)
				{
					MaterialCache[Mat.MaterialInterface->PathFileName] = Mat.MaterialInterface;
					if (Mat.MaterialInterface->PathFileName == "None")
						continue;

					FString MatBinPath = FObjManager::GetMBinaryFilePath(Mat.MaterialInterface->PathFileName);

					FWindowsBinWriter MatWriter(MatBinPath);
					if (MatWriter.IsValid())
					{
						Mat.MaterialInterface->Serialize(MatWriter); // UMaterial 데이터 직렬화
					}
				}
			}

			// MaterialIndex 캐싱을 위해 Materials를 먼저 설정
			StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
			StaticMesh->SetStaticMeshAsset(NewMeshAsset);

			// 파싱 결과를 하드디스크에 굽기 (다음 로딩 속도 최적화)
			FWindowsBinWriter Writer(BinPath);
			if (Writer.IsValid())
			{
				StaticMesh->Serialize(Writer);
			}
		}
	}

	StaticMesh->InitResources(InDevice);

	// 캐시 등록
	StaticMeshCache[CacheKey] = StaticMesh;

	ScanMeshAssets();
	ScanMaterialAssets();

	return StaticMesh;
}


UMaterial* FObjManager::GetOrLoadMaterial(const FString& MaterialName)
{
	// 머티리얼 슬롯 이름을 그대로 캐시 키로 사용
	// stem()을 쓰면 "MatID_1.001" 같은 블렌더 이름에서 ".001"이 확장자로 잘려나감
	const FString& FileNameOnly = MaterialName;

	// 1. 캐시(RAM)에 이미 있는지 검사
	if (MaterialCache.contains(FileNameOnly))
	{
		UE_LOG("Cached MaterialName: %s;", FileNameOnly.c_str());
		return MaterialCache[FileNameOnly];
	}

	// 2. 캐시에 없다면 빈 객체 생성
	UMaterial* NewMaterial = UObjectManager::Get().CreateObject<UMaterial>();
	UE_LOG("Cache Missed MaterialName: %s;", FileNameOnly.c_str());

	FString MBinPath = GetMBinaryFilePath(MaterialName);
	std::filesystem::path MBinPathW = FPaths::ToWide(MBinPath);
	std::filesystem::path SrcPath = FPaths::ToWide(MaterialName);
	bool bNeedRebuild = true;

	if (std::filesystem::exists(MBinPathW))
	{
		// 원본 파일이 없거나, 이미 mbin 경로가 들어왔거나,
		// mbin 파일의 수정 날짜가 원본 파일보다 최신(또는 같음)인 경우 리빌드 생략
		if (!std::filesystem::exists(SrcPath) || MaterialName == MBinPath ||
			std::filesystem::last_write_time(MBinPathW) >= std::filesystem::last_write_time(SrcPath))
		{
			bNeedRebuild = false;
		}
	}

	if (!bNeedRebuild)
	{
		// MBIN 파일에서 통째로 로드
		FWindowsBinReader Reader(MBinPath);
		if (Reader.IsValid())
		{
			NewMaterial->Serialize(Reader);
		}
		else
		{
			bNeedRebuild = true; // 파일 읽기 실패 시 강제 리빌드로 전환
		}
	}

	// 3. 하드디스크(.bin)에 있다면 로드
	if (bNeedRebuild)
	{
		// [리빌드 진행 영역]
		// 현재 첨부된 코드 상에는 ObjImporter에서 Material을 같이 추출하지만,
		// 추후 Material 단독 파일(예: .mat, .json 등)을 파싱하는 전용 Importer가 있다면 이 부분에서 파싱을 수행해야 합니다.

		// 기존 코드 동작 유지 보장: 파일이 존재하면 기존처럼 읽어들이기 시도
		if (std::filesystem::exists(SrcPath))
		{
			FWindowsBinReader Reader(MaterialName);
			if (Reader.IsValid())
			{
				NewMaterial->Serialize(Reader);
			}
		}

		// 속성이 실제로 로드된 경우에만 .mbin 저장 (빈 머티리얼로 .mbin을 덮어쓰지 않음)
		if (!NewMaterial->PathFileName.empty())
		{
			FWindowsBinWriter Writer(MBinPath);
			if (Writer.IsValid())
			{
				NewMaterial->Serialize(Writer);
			}
		}
	}

	// 4. 캐시에 등록 후 반환
	MaterialCache[FileNameOnly] = NewMaterial;
	return NewMaterial;
}