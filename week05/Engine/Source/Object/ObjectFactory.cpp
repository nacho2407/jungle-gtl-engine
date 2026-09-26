#include "ObjectFactory.h"

#include "Object/Class.h"
#include "Object/Object.h"

TMap<uint32, UObject*> GUUIDToObjectMap;
uint32 FObjectFactory::LastUUID = 0;

UObject* FObjectFactory::ConstructObject(const FObjectCreateParams& Params)
{
	if (!Params.Class)
	{
		return nullptr;
	}

	UObject* NewObj = Params.Class->CreateInstance(Params.Outer, Params.Name);
	if (!NewObj)
	{
		return nullptr;
	}

	NewObj->UUID = GenerateUUID();
	NewObj->InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(NewObj);
	GUUIDToObjectMap[NewObj->UUID] = NewObj;

	if (Params.Flags != EObjectFlags::None)
	{
		NewObj->AddFlags(Params.Flags);
	}

	NewObj->PostConstruct();
	return NewObj;
}

UObject* FObjectFactory::ConstructObject(
	UClass* InClass,
	UObject* InOuter,
	const FString& InName,
	EObjectFlags InFlags)
{
	FObjectCreateParams Params;
	Params.Class = InClass;
	Params.Outer = InOuter;
	Params.Name = InName;
	Params.Flags = InFlags;
	return ConstructObject(Params);
}

uint32 FObjectFactory::GetLastUUID()
{
	return LastUUID;
}

void FObjectFactory::SetLastUUID(uint32 InUUID)
{
	LastUUID = InUUID;
}

uint32 FObjectFactory::GenerateUUID()
{
	return ++LastUUID;
}
