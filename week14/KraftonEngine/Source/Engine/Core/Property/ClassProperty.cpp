#include "ClassProperty.h"

#include "Object/UClass.h"
#include "Serialization/Archive.h"

UClass* FClassProperty::GetClassValue(void* Container) const
{
	return GetClassValueFromValuePtr(GetValuePtrFor(Container));
}

void FClassProperty::SetClassValue(void* Container, UClass* Class) const
{
	SetClassValueFromValuePtr(GetValuePtrFor(Container), Class);
}

UClass* FClassProperty::GetClassValueFromValuePtr(void* ValuePtr) const
{
	return ValuePtr && Ops && Ops->GetClass ? Ops->GetClass(ValuePtr) : nullptr;
}

void FClassProperty::SetClassValueFromValuePtr(void* ValuePtr, UClass* Class) const
{
	if (!ValuePtr || !Ops || !Ops->SetClass)
	{
		return;
	}

	UClass* AllowedClass = GetAllowedClassType();
	if (Class && AllowedClass && !Class->IsA(AllowedClass))
	{
		Class = nullptr;
	}

	Ops->SetClass(ValuePtr, Class);
}

void FClassProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	FString ClassName;
	if (Ar.IsSaving())
	{
		UClass* Class = GetClassValueFromValuePtr(ValuePtr);
		ClassName = Class ? FString(Class->GetName()) : FString("None");
	}

	Ar << ClassName;

	if (Ar.IsLoading())
	{
		UClass* Class = (ClassName.empty() || ClassName == "None")
			? nullptr
			: UClass::FindByName(ClassName.c_str());
		SetClassValueFromValuePtr(ValuePtr, Class);
	}
}
