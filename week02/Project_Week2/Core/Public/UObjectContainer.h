#pragma once
#include "CoreTypes.h"
#include "FUObjectFactory.h"
#include "UObject.h"

class FObjectManager
{
	THashMap<uint32, UObject*> ObjectHashMap;
	FUObjectFactory ObjectFactory;
	//THashMap<type_info, TArray

	void RegisterObject(UObject* InUObject);
public:
	UObject* CreateObject(FString Name);
	void DestroyObject(UObject* InUObject);
	void Clear();
};