#include "ObjectProperty.h"

#include "Object/Object.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

UObject* FObjectProperty::GetObjectValue(void* Container) const
{
	return GetObjectValueFromValuePtr(GetValuePtrFor(Container));
}

void FObjectProperty::SetObjectValue(void* Container, UObject* Object) const
{
	SetObjectValueFromValuePtr(GetValuePtrFor(Container), Object);
}

UObject* FObjectProperty::GetObjectValueFromValuePtr(void* ValuePtr) const
{
	return ValuePtr && Ops && Ops->GetObject ? Ops->GetObject(ValuePtr) : nullptr;
}

void FObjectProperty::SetObjectValueFromValuePtr(void* ValuePtr, UObject* Object) const
{
	if (ValuePtr && Ops && Ops->SetObject)
	{
		Ops->SetObject(ValuePtr, Object);
	}
}

json::JSON FObjectProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	UObject* Object = GetObjectValueFromValuePtr(ValuePtr);
	return Object ? JSON(static_cast<int>(Object->GetUUID())) : JSON();
}

void FObjectProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	const uint32 UUID = static_cast<uint32>(Value.ToInt());
	SetObjectValueFromValuePtr(ValuePtr, UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr);
}

void FObjectProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	uint32 UUID = 0;
	if (Ar.IsSaving())
	{
		UObject* Object = GetObjectValueFromValuePtr(ValuePtr);
		UUID = Object ? Object->GetUUID() : 0;
	}

	Ar << UUID;

	if (Ar.IsLoading())
	{
		SetObjectValueFromValuePtr(ValuePtr, UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr);
	}
}
