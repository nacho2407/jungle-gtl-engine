#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/String.h"
#include "Serialization/Archive.h"

class UObject;
class UClass;

class FObjectGraphReferenceResolver final : public IObjectReferenceResolver
{
public:
	uint32 GetObjectId(UObject* Object) const override;
	UObject* ResolveObjectId(uint32 Id, UClass* ExpectedClass) const override;

	void AddObject(uint32 Id, UObject* Object);
	void AddObjectId(UObject* Object, uint32 Id);
	void Clear();

private:
	TMap<uint32, UObject*> IdToObject;
	TMap<UObject*, uint32> ObjectToId;
};

class FObjectGraphSerializer
{
public:
	bool SaveToFile(const FString& Path, UObject* RootObject, const FString& AssetType);
	UObject* LoadFromFile(const FString& Path, const FString& ExpectedRootType);

private:
	void Clear();
	void CollectGraph(UObject* RootObject);
	uint32 GetOrAssignObjectId(UObject* Object);
	void DestroyCreatedObjects(UObject* RootObject, bool bPropertiesLinked);

	TArray<UObject*> Objects;
	TMap<UObject*, uint32> ObjectToId;
	FObjectGraphReferenceResolver Resolver;
};
