#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/ObjImporter.h"
#include "Materials/Material.h"
#include "Serialization/WindowsArchive.h"
#include "Engine/Platform/Paths.h"
#include <filesystem>

std::map<FString, UStaticMesh*> FObjManager::StaticMeshCache;
TMap<FString, UMaterial*> FObjManager::MaterialCache;
TArray<FMeshAssetListItem> FObjManager::AvailableMeshFiles;

FString FObjManager::GetBinaryFilePath(const FString& OriginalPath)
{
	std::filesystem::path SrcPath(OriginalPath);
	std::wstring Ext = SrcPath.extension().wstring();

	// 이미 bin 경로가 들어온 경우에는 그대로 사용
	if (Ext == L".bin")
	{
		return OriginalPath;
	}

	// obj 등 원본 메시 경로가 들어온 경우에는 MeshCache 아래에 bin 생성
	std::wstring CacheDir = FPaths::RootDir() + L"Asset\\MeshCache\\";
	FPaths::CreateDir(CacheDir);

	std::filesystem::path BinPath = std::filesystem::path(CacheDir) / SrcPath.stem();
	BinPath += L".bin";

	return FPaths::ToUtf8(BinPath.wstring());
}

void FObjManager::ScanMeshAssets()
{
	AvailableMeshFiles.clear();

	std::map<FString, FMeshAssetListItem> MergedItems;

	auto AddFilesFromRoot = [](const std::filesystem::path& RootPath, const wchar_t* TargetExt, std::map<FString, FMeshAssetListItem>& OutMap)
		{
			if (!std::filesystem::exists(RootPath))
			{
				return;
			}

			for (const auto& Entry : std::filesystem::recursive_directory_iterator(RootPath))
			{
				if (!Entry.is_regular_file())
				{
					continue;
				}

				const std::filesystem::path& Path = Entry.path();
				if (Path.extension() != TargetExt)
				{
					continue;
				}

				FString Key = FPaths::ToUtf8(Path.stem().wstring());

				FMeshAssetListItem Item;
				Item.DisplayName = Key;
				Item.FullPath = FPaths::ToUtf8(Path.wstring());

				OutMap[Key] = Item;
			}
		};

	const std::filesystem::path DataRoot = FPaths::RootDir() + L"Data\\";
	const std::filesystem::path MeshCacheRoot = FPaths::RootDir() + L"Asset\\MeshCache\\";

	// bin이 같은 키를 덮어쓰도록 우선적용
	AddFilesFromRoot(DataRoot, L".obj", MergedItems);
	AddFilesFromRoot(MeshCacheRoot, L".bin", MergedItems);

	AvailableMeshFiles.reserve(MergedItems.size());
	for (auto& [Key, Item] : MergedItems)
	{
		AvailableMeshFiles.push_back(std::move(Item));
	}
}

const TArray<FMeshAssetListItem>& FObjManager::GetAvailableMeshFiles()
{
	return AvailableMeshFiles;
}


UStaticMesh* FObjManager::LoadObjStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
	// 캐시 확인 (O(1) 룩업)
	auto It = StaticMeshCache.find(PathFileName);
	if (It != StaticMeshCache.end())
	{
		return It->second;
	}

	// UStaticMesh 생성 + FStaticMesh 소유권 이전 + 머티리얼 설정
	UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();

	FString BinPath = GetBinaryFilePath(PathFileName);
	bool bNeedRebuild = true;

	// 3. 타임스탬프 비교 (디스크 캐시 확인)
	if (std::filesystem::exists(BinPath) && std::filesystem::exists(PathFileName))
	{
		if (std::filesystem::last_write_time(BinPath) >= std::filesystem::last_write_time(PathFileName))
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
			NewMeshAsset->PathFileName = PathFileName;
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
	StaticMeshCache[PathFileName] = StaticMesh;

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