#include "StructProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/Types/CoreTypes.h"
#include "Serialization/Archive.h"
#include "Object/Reflection/UStruct.h"


json::JSON FStructProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	JSON obj = json::Object();
	if (!ValuePtr || !StructType)
	{
		return obj;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		obj[Child->Name] = Child->Serialize(ValuePtr);
	}
	return obj;
}

json::JSON FStructProperty::SerializeValue(void* ValuePtr, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	JSON obj = json::Object();
	if (!ValuePtr || !StructType)
	{
		return obj;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		obj[Child->Name] = Child->Serialize(ValuePtr, RefContext);
	}
	return obj;
}

void FStructProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child || !Child->Name || !Value.hasKey(Child->Name))
		{
			continue;
		}

		json::JSON& ChildValue = Value[Child->Name];
		Child->Deserialize(ValuePtr, ChildValue);
	}
}

void FStructProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, const FJsonObjectReferenceContext* RefContext) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child || !Child->Name || !Value.hasKey(Child->Name))
		{
			continue;
		}

		json::JSON& ChildValue = Value[Child->Name];
		Child->Deserialize(ValuePtr, ChildValue, RefContext);
	}
}

json::JSON FStructProperty::SerializeValue(void* ValuePtr, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	JSON obj = json::Object();
	if (!ValuePtr || !StructType)
	{
		return obj;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		obj[Child->Name] = Child->SerializeValue(Child->GetValuePtrFor(ValuePtr), Owner, RefContext);
	}
	return obj;
}

void FStructProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child || !Child->Name || !Value.hasKey(Child->Name))
		{
			continue;
		}

		json::JSON& ChildValue = Value[Child->Name];
		Child->DeserializeValue(Child->GetValuePtrFor(ValuePtr), ChildValue, Owner, RefContext);
	}
}

void FStructProperty::SerializeValue(void* ValuePtr, UObject* Owner, FArchive& Ar) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		Child->SerializeValue(Child->GetValuePtrFor(ValuePtr), Owner, Ar);
	}
}

void FStructProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr || !StructType)
	{
		return;
	}

	TArray<const FProperty*> Children;
	StructType->GetPropertyRefs(Children);
	for (const FProperty* Child : Children)
	{
		if (!Child)
		{
			continue;
		}
		Child->Serialize(ValuePtr, Ar);
	}
}
