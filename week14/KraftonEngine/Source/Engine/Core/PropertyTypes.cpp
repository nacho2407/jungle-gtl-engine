#include "Core/PropertyTypes.h"

#include "SimpleJSON/json.hpp"
#include "Object/UStruct.h"

bool FJsonObjectReferenceContext::SerializeObjectReference(const UObject* Object, json::JSON& OutValue) const
{
	(void)Object;
	(void)OutValue;
	return false;
}

bool FJsonObjectReferenceContext::DeserializeObjectReference(json::JSON& Value, UObject*& OutObject) const
{
	(void)Value;
	OutObject = nullptr;
	return false;
}

const char* FPropertyValue::GetName() const
{
	return Property && Property->Name ? Property->Name : "";
}

const char* FPropertyValue::GetDisplayName() const
{
	return Property && Property->DisplayName ? Property->DisplayName : GetName();
}

const char* FPropertyValue::GetCategory() const
{
	return Property && Property->Category ? Property->Category : "";
}

EPropertyType FPropertyValue::GetType() const
{
	return Property ? Property->GetType() : EPropertyType::Bool;
}

float FPropertyValue::GetMin() const
{
	return Property ? Property->GetMin() : 0.0f;
}

float FPropertyValue::GetMax() const
{
	return Property ? Property->GetMax() : 0.0f;
}

float FPropertyValue::GetSpeed() const
{
	return Property ? Property->GetSpeed() : 0.1f;
}

UStruct* FPropertyValue::GetStructType() const
{
	return Property ? Property->GetStructType() : nullptr;
}

const FEnum* FPropertyValue::GetEnumType() const
{
	return Property ? Property->GetEnumType() : nullptr;
}

const TMap<FString, FString>& FPropertyValue::GetMetadata() const
{
	static const TMap<FString, FString> EmptyMetadata;
	return Property ? Property->Metadata : EmptyMetadata;
}

void* FPropertyValue::GetValuePtr() const
{
	return Property ? Property->GetValuePtrFor(ContainerPtr) : nullptr;
}

void FPropertyValue::GetStructChildren(TArray<FPropertyValue>& OutProps) const
{
	OutProps.clear();
	UStruct* StructType = GetStructType();
	void* ValuePtr = GetValuePtr();
	if (!StructType || !ValuePtr)
	{
		return;
	}

	TArray<const FProperty*> ChildProperties;
	StructType->GetPropertyRefs(ChildProperties);
	for (const FProperty* ChildProperty : ChildProperties)
	{
		if (!ChildProperty || !ChildProperty->GetValuePtrFor(ValuePtr))
		{
			continue;
		}

		OutProps.push_back(ChildProperty->ToValue(ValuePtr, Object));
	}
}

json::JSON FProperty::Serialize(UObject* Object) const
{
	return SerializeValue(GetValuePtrFor(Object), Object, nullptr);
}

void FProperty::Deserialize(UObject* Object, json::JSON& JsonValue) const
{
	DeserializeValue(GetValuePtrFor(Object), JsonValue, Object, nullptr);
}

json::JSON FProperty::Serialize(UObject* Object, const FJsonObjectReferenceContext* RefContext) const
{
	return SerializeValue(GetValuePtrFor(Object), Object, RefContext);
}

void FProperty::Deserialize(UObject* Object, json::JSON& JsonValue, const FJsonObjectReferenceContext* RefContext) const
{
	DeserializeValue(GetValuePtrFor(Object), JsonValue, Object, RefContext);
}

void FProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	SerializeValue(GetValuePtrFor(Object), Object, Ar);
}

json::JSON FProperty::Serialize(void* Container) const
{
	return SerializeValue(GetValuePtrFor(Container));
}

void FProperty::Deserialize(void* Container, json::JSON& JsonValue) const
{
	DeserializeValue(GetValuePtrFor(Container), JsonValue);
}

json::JSON FProperty::Serialize(void* Container, const FJsonObjectReferenceContext* RefContext) const
{
	return SerializeValue(GetValuePtrFor(Container), RefContext);
}

void FProperty::Deserialize(void* Container, json::JSON& JsonValue, const FJsonObjectReferenceContext* RefContext) const
{
	DeserializeValue(GetValuePtrFor(Container), JsonValue, RefContext);
}

json::JSON FProperty::SerializeValue(void* ValuePtr, const FJsonObjectReferenceContext* RefContext) const
{
	(void)RefContext;
	return SerializeValue(ValuePtr);
}

void FProperty::DeserializeValue(void* ValuePtr, json::JSON& JsonValue, const FJsonObjectReferenceContext* RefContext) const
{
	(void)RefContext;
	DeserializeValue(ValuePtr, JsonValue);
}

json::JSON FProperty::SerializeValue(void* ValuePtr, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	(void)Owner;
	return SerializeValue(ValuePtr, RefContext);
}

void FProperty::DeserializeValue(void* ValuePtr, json::JSON& JsonValue, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	(void)Owner;
	DeserializeValue(ValuePtr, JsonValue, RefContext);
}

void FProperty::SerializeValue(void* ValuePtr, UObject* Owner, FArchive& Ar) const
{
	(void)Owner;
	SerializeValue(ValuePtr, Ar);
}

void FProperty::Serialize(void* Container, FArchive& Ar) const
{
	SerializeValue(GetValuePtrFor(Container), Ar);
}
