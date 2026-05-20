#include "ObjectProperty.h"

#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/Reflection/UClass.h"
#include "Serialization/Archive.h"
#include <cstring>

namespace
{
	const char* InstancedClassNameKey = "ClassName";
	const char* InstancedPropertiesKey = "Properties";
}

UObject* FObjectProperty::GetObjectValue(void* Container) const
{
	return GetObjectValueFromValuePtr(GetValuePtrFor(Container));
}

void FObjectProperty::SetObjectValue(void* Container, UObject* Object) const
{
	SetObjectValueFromValuePtr(GetValuePtrFor(Container), Object);
}

UObject* FObjectProperty::GetObjectValueFromValuePtr(void* ValuePtr) const
{
	return ValuePtr && Ops && Ops->GetObject ? Ops->GetObject(ValuePtr) : nullptr;
}

void FObjectProperty::SetObjectValueFromValuePtr(void* ValuePtr, UObject* Object) const
{
	if (ValuePtr && Ops && Ops->SetObject)
	{
		Ops->SetObject(ValuePtr, Object);
	}
}

void FObjectProperty::SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const
{
	if ((Flags & PF_InstancedReference) == 0)
	{
		SerializeValue(ValuePtr, Ar);
		return;
	}

	UObject* InstancedObject = nullptr;
	FString ClassName;
	if (Ar.IsSaving())
	{
		InstancedObject = GetObjectValueFromValuePtr(ValuePtr);
		ClassName = InstancedObject ? FString(InstancedObject->GetClass()->GetName()) : FString("None");
	}

	Ar.BeginObject();
	Ar.BeginProperty(InstancedClassNameKey);
	Ar << ClassName;
	Ar.EndProperty();

	if (Ar.IsLoading())
	{
		if (ClassName.empty() || ClassName == "None")
		{
			SetObjectValueFromValuePtr(ValuePtr, nullptr);
			Ar.EndObject();
			return;
		}

		InstancedObject = GetObjectValueFromValuePtr(ValuePtr);
		if (!InstancedObject || std::strcmp(InstancedObject->GetClass()->GetName(), ClassName.c_str()) != 0)
		{
			UObject* NewObject = FObjectFactory::Get().Create(ClassName, Context.Owner);
			if (NewObject)
			{
				UClass* AllowedClass = GetAllowedClassType();
				if (!AllowedClass || NewObject->GetClass()->IsA(AllowedClass))
				{
					InstancedObject = NewObject;
					SetObjectValueFromValuePtr(ValuePtr, InstancedObject);
				}
				else
				{
					UObjectManager::Get().DestroyObject(NewObject);
					InstancedObject = nullptr;
				}
			}
		}
		else
		{
			InstancedObject->SetOuter(Context.Owner);
		}
	}

	if (InstancedObject)
	{
		Ar.BeginProperty(InstancedPropertiesKey);
		InstancedObject->SerializeProperties(Ar, PF_Save);
		Ar.EndProperty();
	}

	Ar.EndObject();
}

void FObjectProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	if ((Flags & PF_InstancedReference) == 0)
	{
		FProperty::Serialize(Object, Ar);
		return;
	}

	FPropertySerializeContext Context;
	Context.Owner = Object;
	SerializeValue(GetValuePtrFor(Object), Ar, Context);
}

void FObjectProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!Ar.UsesCustomObjectReferenceSerialization())
	{
		uint32 UUID = 0;
		if (Ar.IsSaving())
		{
			UObject* Object = GetObjectValueFromValuePtr(ValuePtr);
			UUID = Object ? Object->GetUUID() : 0;
		}

		Ar << UUID;

		if (Ar.IsLoading())
		{
			UObject* ResolvedObject = Ar.ResolveObjectReference(UUID);
			SetObjectValueFromValuePtr(ValuePtr, ResolvedObject ? ResolvedObject : (UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr));
			if (Ar.IsObjectReferenceRemapping() && UUID != 0 && !ResolvedObject)
			{
				Ar.AddObjectReferenceFixup(
					UUID,
					[this, ValuePtr](UObject* Duplicate)
					{
						if (Duplicate)
						{
							SetObjectValueFromValuePtr(ValuePtr, Duplicate);
						}
					}
				);
			}
		}
		return;
	}

	UObject* Object = Ar.IsSaving() ? GetObjectValueFromValuePtr(ValuePtr) : nullptr;
	Ar.SerializeObjectReference(Object);
	if (Ar.IsLoading())
	{
		SetObjectValueFromValuePtr(ValuePtr, Object);
	}
}
