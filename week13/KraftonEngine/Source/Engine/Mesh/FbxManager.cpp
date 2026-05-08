#include "FbxManager.h"
#include "FbxImporter.h"
#include "SkeletalMesh.h"
#include "Core/Log.h"
#include "Platform/Paths.h"

#include <filesystem>

TMap<FString, USkeletalMesh*> FFbxManager::SkeletalMeshCache;
TArray<FFbxListItem> FFbxManager::AvailableFbxFiles;

USkeletalMesh* FFbxManager::LoadFbxSkeletalMesh(const FString& FilePath, ID3D11Device* InDevice)
{
	if (SkeletalMeshCache.find(FilePath) != SkeletalMeshCache.end())
	{
		return SkeletalMeshCache[FilePath];
	}

	FSkeletalMesh* SkeletalMeshAsset = nullptr;
	if (!LoadSkeletalMeshAsset(FilePath, InDevice, SkeletalMeshAsset))
	{
		return nullptr;
	}
	USkeletalMesh* SkeletalMesh = new USkeletalMesh();
	SkeletalMesh->SetSkeletalMaterials(std::move(FFbxImporter::SkeletalMaterials));
	SkeletalMesh->SetSkeletalMeshAsset(SkeletalMeshAsset);

	SkeletalMesh->InitResources(InDevice);

	SkeletalMeshCache[FilePath] = SkeletalMesh;

	return SkeletalMesh;
}

bool FFbxManager::LoadSkeletalMeshAsset(const FString& FilePath, ID3D11Device* InDevice,
	FSkeletalMesh*& OutMesh)
{
	if (!FFbxImporter::Import(FilePath))
	{
		UE_LOG("Failed to import FBX file: %s", FilePath.c_str());
		return false;
	}

	OutMesh = new FSkeletalMesh();
	OutMesh->PathFileName = FilePath;
	OutMesh->Vertices = std::move(FFbxImporter::Vertices);
	OutMesh->Indices = std::move(FFbxImporter::Indices);
	OutMesh->Sections = std::move(FFbxImporter::Sections);
	OutMesh->Bones = std::move(FFbxImporter::Bones);

	return true;
}

void FFbxManager::ScanFbxSourceFiles()
{
	AvailableFbxFiles.clear();

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
			
		FFbxListItem Item;
		Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
		Item.FullPath = FPaths::ToUtf8(Path.lexically_relative(ProjectRoot).generic_wstring());
		AvailableFbxFiles.push_back(std::move(Item));
	}
}
