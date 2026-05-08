#include "FbxManager.h"
#include "FbxImporter.h"
#include "SkeletalMesh.h"
#include "Core/Log.h"

USkeletalMesh* FFbxManager::LoadFbxSkeletalMesh(const FString& FilePath, ID3D11Device* InDevice)
{
	FSkeletalMesh* SkeletalMeshAsset = nullptr;
	if (!LoadSkeletalMeshAsset(FilePath, InDevice, SkeletalMeshAsset))
	{
		return nullptr;
	}
	USkeletalMesh* SkeletalMesh = new USkeletalMesh();
	SkeletalMesh->SetSkeletalMeshAsset(SkeletalMeshAsset);
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
	OutMesh->Bones = std::move(FFbxImporter::Bones);

	return true;
}
