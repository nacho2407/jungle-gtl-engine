#include "StructProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"
#include "Object/UStruct.h"


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

json::JSON FStructProperty::SerializeValue(void* ValuePtr, const FPropertySerializeContext& Context) const
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
		obj[Child->Name] = Child->SerializeValue(Child->GetValuePtrFor(ValuePtr), Context);
	}
	return obj;
}

void FStructProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, const FPropertySerializeContext& Context) const
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
		Child->DeserializeValue(Child->GetValuePtrFor(ValuePtr), ChildValue, Context);
	}
}

void FStructProperty::SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const
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
		Child->SerializeValue(Child->GetValuePtrFor(ValuePtr), Ar, Context);
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
