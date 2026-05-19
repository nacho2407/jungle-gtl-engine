#include "Object.h"
#include "EngineStatics.h"
#include "Object/FName.h"
#include "Object/Class.h"
#include "Object/Function.h"
#include "Object/Property.h"
#include "Core/Reflection/ReflectionRegistry.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
	: UObject(true)
{
}

UObject::UObject(bool bRegisterObject)
{
	UUID = EngineStatics::GenUUID();
	bRegisteredWithObjectArray = bRegisterObject;
	if (bRegisteredWithObjectArray)
	{
		InternalIndex = static_cast<uint32>(GUObjectArray.size());
		GUObjectArray.push_back(this);
	}
}

UObject::~UObject()
{
	if (!bRegisteredWithObjectArray || GUObjectArray.empty())
	{
		return;
	}

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

const char* UObject::GetClassName() const
{
	UClass* Class = GetClass();
	return Class ? Class->GetName() : "UObject";
}

void UObjectManager::AssignObjectName(UObject* Obj, const UClass* Class)
{
	if (!Obj)
	{
		return;
	}

	const char* ClassName = Class ? Class->GetName() : "UObject";
	uint32& Counter = NameCounters[ClassName];
	FString Name = FString(ClassName) + "_" + std::to_string(Counter++);
	Obj->SetFName(FName(Name));
}

bool UObject::IsA(const UClass* Class) const
{
	UClass* ThisClass = GetClass();
	return ThisClass && ThisClass->IsChildOf(Class);
}

void UObject::ProcessEvent(UFunction* Function, void* Params)
{
	if (Function)
	{
		Function->Invoke(this, Params);
	}
}

UObject* NewObject(UClass* Class)
{
	if (!Class || Class->HasAnyClassFlags(CF_Abstract))
		return nullptr;

	return Class->CreateObject();
}

UObject* UObject::Duplicate(const FDuplicateContext* Context)
{
	UObject* Dup = NewObject(GetClass());

	if (!Dup)
		return nullptr;

	Dup->CopyPropertiesFrom(this, Context);
	Dup->PostDuplicate(this);
	return Dup;
}

void UObject::CopyPropertiesFrom(UObject* Src, const FDuplicateContext* Context)
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

		if (DstProperty->Type == EPropertyType::Struct && DstProperty->ScriptStruct != SrcProperty->ScriptStruct)
			continue;

		if (DstProperty->CopyValue(this, Src, Context))
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
	SerializeProperties(Ar);
}

void UObject::SerializeProperties(FArchive& Ar)
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
