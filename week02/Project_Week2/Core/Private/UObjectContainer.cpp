#include "UObjectContainer.h"



UObject* FObjectManager::CreateObject(FString Name)
{
	auto NewObject = ObjectFactory.ConstructObject(Name);

	RegisterObject(NewObject);
	return NewObject; 
}

void FObjectManager::RegisterObject(UObject* InUObject)
{
	if (ObjectHashMap.contains(InUObject->GetUUID()))
	{
		static_assert("SameUUID inserted");
		return;
	}

	ObjectHashMap[InUObject->GetUUID()] = InUObject;
}

void FObjectManager::Clear()
{
	for (const auto& [key, value] : ObjectHashMap)
	{
		delete value;
	}

	ObjectHashMap.clear();
}

void FObjectManager::DestroyObject(UObject* InUObject)
{
	if (ObjectHashMap.contains(InUObject->GetUUID()))
	{
		static_assert("UnManaged Object");
		return;
	}

	ObjectHashMap.erase(InUObject->GetUUID());
}
