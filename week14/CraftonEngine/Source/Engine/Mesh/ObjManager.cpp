#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Engine/Platform/Paths.h"
#include <filesystem>

std::map<FString, UStaticMesh*> FObjManager::StaticMeshCache;
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

	StaticMesh->Build(PathFileName, InDevice);

	// 캐시 등록
	StaticMeshCache[PathFileName] = StaticMesh;

	return StaticMesh;
}