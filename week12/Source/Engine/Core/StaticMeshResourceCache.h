#pragma once

#include "Asset/StaticMesh.h"
#include "Core/CoreTypes.h"
#include "Core/ResourceTypes.h"

class FStaticMeshResourceCache
{
public:
	void RegisterResource(const FStaticMeshResource& Resource);
	void ClearRegistry();
	const TMap<FString, FStaticMeshResource>& GetRegistry() const;
	FStaticMeshLoadOptions GetLoadOptions(const FString& Path) const;

	UStaticMesh* Find(const FString& Path) const;
	void RegisterLoaded(const FString& Path, UStaticMesh* StaticMesh);
	void Release();

private:
	TMap<FString, FStaticMeshResource> StaticMeshRegistry;
	TMap<FString, UStaticMesh*> StaticMeshes;
};
