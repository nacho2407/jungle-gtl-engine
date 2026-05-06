#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
class UStaticMesh;

class FStaticMeshLoadService
{
public:
	explicit FStaticMeshLoadService(FResourceManager& InResourceManager);

	UStaticMesh* Load(const FString& Path);

private:
	UStaticMesh* LoadMissingObjBinaryFallback(const FString& RequestedPath, const FString& BinaryPath);
	UStaticMesh* LoadBinaryDrop(const FString& NormalizedPath);
	UStaticMesh* LoadObjOrCachedBinary(const FString& NormalizedPath);

	FResourceManager& ResourceManager;
};
