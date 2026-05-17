#include "StructProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"
#include "Object/UStruct.h"


json::JSON FStructProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
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

void FStructProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
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

void FStructProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
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