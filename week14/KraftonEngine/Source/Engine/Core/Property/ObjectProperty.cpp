#include "ObjectProperty.h"

#include "Object/Object.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

UObject* FObjectProperty::GetObjectValue(void* Container) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr && Ops && Ops->GetObject ? Ops->GetObject(ValuePtr) : nullptr;
}

void FObjectProperty::SetObjectValue(void* Container, UObject* Object) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr && Ops && Ops->SetObject)
	{
		Ops->SetObject(ValuePtr, Object);
	}
}

json::JSON FObjectProperty::Serialize(void* Container) const
{
	using namespace json;

	UObject* Object = GetObjectValue(Container);
	return Object ? JSON(static_cast<int>(Object->GetUUID())) : JSON();
}

void FObjectProperty::Deserialize(void* Container, json::JSON& Value) const
{
	const uint32 UUID = static_cast<uint32>(Value.ToInt());
	SetObjectValue(Container, UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr);
}

void FObjectProperty::Serialize(void* Container, FArchive& Ar) const
{
	uint32 UUID = 0;
	if (Ar.IsSaving())
	{
		UObject* Object = GetObjectValue(Container);
		UUID = Object ? Object->GetUUID() : 0;
	}

	Ar << UUID;

	if (Ar.IsLoading())
	{
		SetObjectValue(Container, UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr);
	}
}
