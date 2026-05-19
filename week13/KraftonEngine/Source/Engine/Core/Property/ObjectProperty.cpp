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

json::JSON FObjectProperty::SerializeValue(void* ValuePtr, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	UObject* Object = GetObjectValueFromValuePtr(ValuePtr);
	if (RefContext)
	{
		JSON RefValue;
		if (RefContext->SerializeObjectReference(Object, RefValue))
		{
			return RefValue;
		}
	}

	return SerializeValue(ValuePtr);
}

void FObjectProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, const FJsonObjectReferenceContext* RefContext) const
{
	if (RefContext)
	{
		UObject* Object = nullptr;
		if (RefContext->DeserializeObjectReference(Value, Object))
		{
			SetObjectValueFromValuePtr(ValuePtr, Object);
			return;
		}
	}

	DeserializeValue(ValuePtr, Value);
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
		UObject* ResolvedObject = Ar.ResolveObjectReference(UUID);
		SetObjectValueFromValuePtr(ValuePtr, ResolvedObject ? ResolvedObject : (UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr));
		if (Ar.IsObjectReferenceRemapping() && UUID != 0 && !ResolvedObject)
		{
			Ar.AddObjectReferenceFixup(
				UUID,
				[this, ValuePtr](UObject* Duplicate)
				{
					if (Duplicate)
					{
						SetObjectValueFromValuePtr(ValuePtr, Duplicate);
					}
				}
			);
		}
	}
}
