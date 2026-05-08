#pragma once
#include "Core/CoreTypes.h"
#include "Render/Types/RenderTypes.h"

struct FSkeletalMesh;
class USkeletalMesh;

class FFbxManager
{
public:
	static USkeletalMesh* LoadFbxSkeletalMesh(const FString& FilePath, ID3D11Device* InDevice);

private:
	static bool LoadSkeletalMeshAsset(const FString& FilePath, ID3D11Device* InDevice,
		FSkeletalMesh*& OutMesh);
};