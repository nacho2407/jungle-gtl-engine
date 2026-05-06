#include "Core/StaticMeshResourceCache.h"

#include "Core/Paths.h"
#include "Object/ObjectFactory.h"

#include <unordered_set>

void FStaticMeshResourceCache::RegisterResource(const FStaticMeshResource& Resource)
{
	StaticMeshRegistry[Resource.Name] = Resource;
}

void FStaticMeshResourceCache::ClearRegistry()
{
	StaticMeshRegistry.clear();
}

const TMap<FString, FStaticMeshResource>& FStaticMeshResourceCache::GetRegistry() const
{
	return StaticMeshRegistry;
}

FStaticMeshLoadOptions FStaticMeshResourceCache::GetLoadOptions(const FString& Path) const
{
	FStaticMeshLoadOptions LoadOptions = {};
	const FString NormalizedPath = FPaths::Normalize(Path);
	for (const auto& [Key, Resource] : StaticMeshRegistry)
	{
		if (Resource.Path == NormalizedPath)
		{
			LoadOptions.bNormalizeToUnitCube = Resource.bNormalizeToUnitCube;
			break;
		}
	}
	return LoadOptions;
}

UStaticMesh* FStaticMeshResourceCache::Find(const FString& Path) const
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	auto It = StaticMeshes.find(NormalizedPath);
	if (It == StaticMeshes.end())
	{
		return nullptr;
	}

	return It->second;
}

void FStaticMeshResourceCache::RegisterLoaded(const FString& Path, UStaticMesh* StaticMesh)
{
	const FString NormalizedPath = FPaths::Normalize(Path);
	if (NormalizedPath.empty() || StaticMesh == nullptr)
	{
		return;
	}

	StaticMeshes[NormalizedPath] = StaticMesh;
}

void FStaticMeshResourceCache::Release()
{
	std::unordered_set<UObject*> DestroyedObjects;
	auto DestroyUniqueObject = [&DestroyedObjects](UObject* Object)
	{
		if (Object && DestroyedObjects.insert(Object).second)
		{
			UObjectManager::Get().DestroyObject(Object);
		}
	};

	for (auto& [Path, StaticMeshAsset] : StaticMeshes)
	{
		DestroyUniqueObject(StaticMeshAsset);
	}
	StaticMeshes.clear();
	StaticMeshRegistry.clear();
}
