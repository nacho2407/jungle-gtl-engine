#include "Core/Types/PropertyTypes.h"

#include "Object/Reflection/UStruct.h"

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

void FProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	FPropertySerializeContext Context;
	Context.Owner = Object;
	SerializeValue(GetValuePtrFor(Object), Ar, Context);
}

void FProperty::SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const
{
	(void)Context;
	SerializeValue(ValuePtr, Ar);
}

void FProperty::Serialize(void* Container, FArchive& Ar) const
{
	SerializeValue(GetValuePtrFor(Container), Ar);
}
