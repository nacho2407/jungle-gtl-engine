#include "Class.h"

UClass::UClass(FString InName, UClass* InSuperClass, CreateFunc InCreateFunc)
	: Name(std::move(InName)),
	SuperClass(InSuperClass),
	Factory(InCreateFunc)
{
	RegisterClass(this);
}

const FString& UClass::GetName() const
{
	return Name;
}

UClass* UClass::GetSuperClass() const
{
	return SuperClass;
}

bool UClass::IsChildOf(const UClass* Other) const
{
    for (const UClass* Current = this; Current != nullptr; Current = Current->SuperClass)
    {
        if (Current == Other)
        {
            return true;
        }
    }
    return false;
}

UObject* UClass::CreateInstance(UObject* InOuter, const FString& InName) const
{
    return Factory ? Factory(InOuter, InName) : nullptr;
}

UClass* UClass::FindClass(const FString& InString)
{
	auto It=GetClassRegistry().find(InString);
	if (It != GetClassRegistry().end())
		return It->second;
	return nullptr;
}

void UClass::RegisterClass(UClass* InClass)
{
	if (InClass)
		GetClassRegistry()[InClass->GetName()] = InClass;
}
TMap<FString, UClass*>& UClass::GetClassRegistry()
{
	static TMap<FString, UClass*> Registry;
	return Registry;
}
