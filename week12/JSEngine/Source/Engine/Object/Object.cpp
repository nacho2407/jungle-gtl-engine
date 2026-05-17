#include "Object.h"
#include "EngineStatics.h"
#include "Object/FName.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Core/Reflection/ReflectionRegistry.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = EngineStatics::GenUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

UObject::~UObject()
{
	uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	if (InternalIndex != LastIndex)
	{
		UObject* LastObject = GUObjectArray[LastIndex];
		GUObjectArray[InternalIndex] = LastObject;
		LastObject->InternalIndex = InternalIndex;
	}

	GUObjectArray.pop_back();
}

UClass* UObject::StaticClass()
{
	static UClass Class(
		"UObject",
		nullptr,
		sizeof(UObject),
		CF_None,
		[]() -> UObject* { return UObjectManager::Get().CreateObject<UObject>(); });

	static bool bRegistered = false;
	if (!bRegistered)
	{
		bRegistered = true;
		FReflectionRegistry::Get().RegisterUClass(&Class);
	}
	return &Class;
}

bool UObject::IsA(const UClass* Class) const
{
	UClass* ThisClass = GetClass();
	return ThisClass && ThisClass->IsChildOf(Class);
}

UObject* NewObject(UClass* Class)
{
	if (!Class || Class->HasAnyClassFlags(CF_Abstract)) 
		return nullptr;

	return Class->CreateObject();
}

UObject* UObject::Duplicate()
{
	UObject* Dup = NewObject(GetClass());

	if (!Dup) 
		return nullptr;

	Dup->CopyPropertiesFrom(this);
	Dup->PostDuplicate(this);
	return Dup;
}

void UObject::CopyPropertiesFrom(UObject* Src)
{
	if (!Src)
		return;

	UClass* SrcClass = Src->GetClass();
	UClass* DstClass = GetClass();

	if (!SrcClass || !DstClass)
		return;

	TArray<const FProperty*> SrcProperties;
	SrcClass->GetAllProperties(SrcProperties);
	for (const FProperty* SrcProperty : SrcProperties)
	{
		if (!SrcProperty || !SrcProperty->Name)
			continue;

		const FProperty* DstProperty = DstClass->FindProperty(SrcProperty->Name);
		if (!DstProperty || DstProperty->Type != SrcProperty->Type)
			continue;

		if (CopyPropertyValue(this, Src, *DstProperty))
		{
			PostEditChangeProperty({ DstProperty->Name, EPropertyChangeType::ValueSet });
		}
	}
}

void UObject::Serialize(FArchive& Ar)
{
	FString ClassName = GetClass() ? GetClass()->GetName() : "UObject";
	Ar << "Type" << ClassName;
	Ar << "ObjectName" << ObjectName;
	SerializeReflectedProperties(Ar);
}

void UObject::SerializeReflectedProperties(FArchive& Ar)
{
	UClass* Class = GetClass();
	if (!Class) 
		return;

	TArray<const FProperty*> Properties;
	Class->GetAllProperties(Properties);
	for (const FProperty* Property : Properties)
	{
		if (!Property)
			continue;
		SerializeProperty(Ar, this, *Property);
	}
}
