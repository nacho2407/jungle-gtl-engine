#include "Object/Class.h"

#include "Object/Object.h"

#include <cstring>

UClass::UClass(const char* InName, UClass* InSuperClass, size_t InClassSize, uint32 InClassFlags,FCreateObjectFunc InCreateFunc)
	: Name(InName), SuperClass(InSuperClass), ClassSize(InClassSize), ClassFlags(InClassFlags), CreateFunc(InCreateFunc)
{
}

bool UClass::IsChildOf(const UClass* Other) const
{
	if (!Other)
	{
		return false;
	}

	for (const UClass* Current = this; Current; Current = Current->SuperClass)
	{
		if (Current == Other)
		{
			return true;
		}
	}
	return false;
}

UObject* UClass::CreateObject() const
{
	return CreateFunc ? CreateFunc() : nullptr;
}

void UClass::AddProperty(const FProperty& Property)
{
	if (!Property.Name || FindProperty(Property.Name))
	{
		return;
	}
	Properties.push_back(Property);
}

const FProperty* UClass::FindProperty(const char* PropertyName) const
{
	if (!PropertyName)
	{
		return nullptr;
	}

	for (const FProperty& Property : Properties)
	{
		if (Property.Name && std::strcmp(Property.Name, PropertyName) == 0)
		{
			return &Property;
		}
	}

	return SuperClass ? SuperClass->FindProperty(PropertyName) : nullptr;
}

void UClass::GetAllProperties(TArray<const FProperty*>& OutProperties) const
{
	if (SuperClass)
	{
		SuperClass->GetAllProperties(OutProperties);
	}

	for (const FProperty& Property : Properties)
	{
		OutProperties.push_back(&Property);
	}
}
