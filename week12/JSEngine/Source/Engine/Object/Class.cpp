#include "Object/Class.h"

#include "Object/Function.h"

#include <cstring>

UField::UField(const char* InName, const char* InDisplayName, const char* InCategory)
	: UObject(false)
	, Name(InName)
	, DisplayName(InDisplayName)
	, Category(InCategory)
{
}

UStruct::UStruct(const char* InName, UStruct* InSuperStruct, size_t InSize, size_t InAlignment, const char* InDisplayName, const char* InCategory)
	: UField(InName, InDisplayName, InCategory)
	, SuperStruct(InSuperStruct)
	, StructureSize(InSize)
	, MinAlignment(InAlignment)
{
}

void UStruct::AddProperty(const FProperty& Property)
{
	if (!Property.Name || FindProperty(Property.Name))
	{
		return;
	}
	Properties.push_back(Property);
}

const FProperty* UStruct::FindProperty(const char* PropertyName) const
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

	return SuperStruct ? SuperStruct->FindProperty(PropertyName) : nullptr;
}

void UStruct::GetAllProperties(TArray<const FProperty*>& OutProperties) const
{
	if (SuperStruct)
	{
		SuperStruct->GetAllProperties(OutProperties);
	}

	for (const FProperty& Property : Properties)
	{
		OutProperties.push_back(&Property);
	}
}

UClass::UClass(const char* InName, UClass* InSuperClass, size_t InClassSize, uint32 InClassFlags, FCreateObjectFunc InCreateFunc, const char* InDisplayName, const char* InCategory)
	: UStruct(InName, InSuperClass, InClassSize, alignof(UObject), InDisplayName, InCategory)
	, ClassFlags(InClassFlags)
	, CreateFunc(InCreateFunc)
{
}

bool UClass::IsChildOf(const UClass* Other) const
{
	if (!Other)
	{
		return false;
	}

	for (const UClass* Current = this; Current; Current = Current->GetSuperClass())
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

void UClass::AddFunction(UFunction* Function)
{
	if (!Function || !Function->GetName())
	{
		return;
	}

	for (UFunction* ExistingFunction : Functions)
	{
		if (ExistingFunction && ExistingFunction->GetName() && std::strcmp(ExistingFunction->GetName(), Function->GetName()) == 0)
		{
			return;
		}
	}

	Functions.push_back(Function);
}

UFunction* UClass::FindFunction(const char* FunctionName) const
{
	if (!FunctionName)
	{
		return nullptr;
	}

	for (UFunction* Function : Functions)
	{
		if (Function && Function->GetName() && std::strcmp(Function->GetName(), FunctionName) == 0)
		{
			return Function;
		}
	}

	UClass* Super = GetSuperClass();
	return Super ? Super->FindFunction(FunctionName) : nullptr;
}

void UClass::GetAllFunctions(TArray<const UFunction*>& OutFunctions) const
{
	if (UClass* Super = GetSuperClass())
	{
		Super->GetAllFunctions(OutFunctions);
	}

	for (const UFunction* Function : Functions)
	{
		if (Function)
		{
			OutFunctions.push_back(Function);
		}
	}
}

UScriptStruct::UScriptStruct(const char* InName, size_t InSize, size_t InAlignment, const IStructOps* InStructOps, const char* InDisplayName, const char* InCategory)
	: UStruct(InName, nullptr, InSize, InAlignment, InDisplayName, InCategory)
	, StructOps(InStructOps)
{
}

void UScriptStruct::Construct(void* Ptr) const
{
	if (StructOps && Ptr)
	{
		StructOps->Construct(Ptr);
	}
}

void UScriptStruct::Destruct(void* Ptr) const
{
	if (StructOps && Ptr)
	{
		StructOps->Destruct(Ptr);
	}
}

void UScriptStruct::Copy(void* Dst, const void* Src) const
{
	if (StructOps && Dst && Src)
	{
		StructOps->Copy(Dst, Src);
	}
}

UEnum::UEnum(const char* InName, uint8 InSize, const FEnumValue* InValues, uint32 InCount, const char* InDisplayName, const char* InCategory)
	: UField(InName, InDisplayName, InCategory)
	, Size(InSize)
	, Values(InValues)
	, Count(InCount)
{
}
