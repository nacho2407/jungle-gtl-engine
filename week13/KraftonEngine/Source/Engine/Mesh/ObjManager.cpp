#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Materials/Material.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include <filesystem>
#include <algorithm>

std::map<FString, UStaticMesh*> FObjManager::StaticMeshCache;
TMap<FString, UMaterial*> FObjManager::MaterialCache;
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;
TArray<FMeshAssetListItem> FObjManager::AvailableObjFiles;

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(FPaths::ToWide(OriginalPath));
	std::wstring Ext = SrcPath.extension().wstring();

	// 이미 bin 경로가 들어온 경우에는 그대로 사용
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	// obj 등 원본 메시 경로가 들어온 경우에는 MeshCache 아래에 bin 생성
	std::wstring CacheDir = FPaths::RootDir() + L"Asset\\MeshCache\\";
	FPaths::CreateDir(CacheDir);

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

	// obj 등 원본 메시 경로가 들어온 경우에는 MeshCache 아래에 bin 생성
	std::wstring CacheDir = FPaths::RootDir() + L"Asset\\MeshCache\\";
	FPaths::CreateDir(CacheDir);

	// 상대 경로로 반환
	std::filesystem::path RelPath = std::filesystem::path(L"Asset\\MeshCache") / SrcPath.stem();
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

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableMeshFiles()
{
	return AvailableMeshFiles;
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

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableObjFiles()
{
	return AvailableObjFiles;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
	// 옵션이 다를 수 있으므로 기존 캐시 무효화
	StaticMeshCache.erase(PathFileName);

	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = GetBinaryFilePath(PathFileName);

	// 항상 리빌드 (옵션이 달라질 수 있음)
	FStaticMesh* NewMeshAsset = new FStaticMesh();
	TArray<FStaticMaterial> ParsedMaterials;

	if (FObjImporter::Import(PathFileName, Options, *NewMeshAsset, ParsedMaterials))
	{
		// 머티리얼 .mbin 저장
		for (auto& Mat : ParsedMaterials)
		{
			if (Mat.MaterialInterface)
			{
				MaterialCache[Mat.MaterialInterface->PathFileName] = Mat.MaterialInterface;
				FString MatBinPath = FObjManager::GetMBinaryFilePath(Mat.MaterialInterface->PathFileName);

				FWindowsBinWriter MatWriter(MatBinPath);
				if (MatWriter.IsValid())
				{
					Mat.MaterialInterface->Serialize(MatWriter);
				}
			}
		}

		NewMeshAsset->PathFileName = PathFileName;
		StaticMesh->SetStaticMeshAsset(NewMeshAsset);
		StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));

		// .bin 저장
		FWindowsBinWriter Writer(BinPath);
		if (Writer.IsValid())
		{
			StaticMesh->Serialize(Writer);
		}
	}

	StaticMesh->InitResources(InDevice);
	StaticMeshCache[PathFileName] = StaticMesh;

	// 리프레시
	ScanMeshAssets();

	return StaticMesh;
}

UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	FString CacheKey = GetBinaryFilePath(PathFileName);

	// 캐시 확인 (O(1) 룩업)
	auto It = StaticMeshCache.find(PathFileName);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	// UStaticMesh 생성 + FStaticMesh 소유권 이전 + 머티리얼 설정
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = CacheKey;
	bool bNeedRebuild = true;

	// 3. 타임스탬프 비교 (디스크 캐시 확인)
	if (std::filesystem::exists(BinPath))
	{
		if (!std::filesystem::exists(PathFileName) || PathFileName == BinPath ||
			std::filesystem::last_write_time(BinPath) >= std::filesystem::last_write_time(PathFileName))
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
		}
		else
		{
			bNeedRebuild = true; // 읽기 실패 시 강제 파싱
		}
	}

	if (bNeedRebuild)
	{
		// 무거운 OBJ 파싱 진행
		FStaticMesh* NewMeshAsset = new FStaticMesh();
		TArray<FStaticMaterial> ParsedMaterials;

		if (FObjImporter::Import(PathFileName, *NewMeshAsset, ParsedMaterials))
		{
			for (auto& Mat : ParsedMaterials)
			{
				if (Mat.MaterialInterface)
				{
					MaterialCache[Mat.MaterialInterface->PathFileName] = Mat.MaterialInterface;
					FString MatBinPath = FObjManager::GetMBinaryFilePath(Mat.MaterialInterface->PathFileName);

					FWindowsBinWriter MatWriter(MatBinPath);
					if (MatWriter.IsValid())
					{
						Mat.MaterialInterface->Serialize(MatWriter); // UMaterial 데이터 직렬화
					}
				}
			}

			StaticMesh->SetStaticMeshAsset(NewMeshAsset);
			StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));

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

	return StaticMesh;
}


UMaterial* FObjManager::GetOrLoadMaterial(const FString& MaterialName)
{
	// 1. 캐시(RAM)에 이미 있는지 검사
	if (MaterialCache.contains(MaterialName))
	{
		return MaterialCache[MaterialName];
	}

	// 2. 캐시에 없다면 빈 객체 생성
	UMaterial* NewMaterial = UObjectManager::Get().CreateObject<UMaterial>();
	FString MatPath = MaterialName;

	// 3. 하드디스크(.bin)에 있다면 로드

	if (std::filesystem::exists(FPaths::ToWide(MatPath)))
	{
		FWindowsBinReader Reader(MatPath);
		if (Reader.IsValid())
		{
			NewMaterial->Serialize(Reader);
		}
	}

	// 4. 캐시에 등록 후 반환
	MaterialCache[MaterialName] = NewMaterial;
	return NewMaterial;
}