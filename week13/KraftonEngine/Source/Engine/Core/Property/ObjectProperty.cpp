#include "ObjectProperty.h"

#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Object/UClass.h"
#include "Object/UStruct.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

namespace
{
	const char* InstancedClassNameKey = "ClassName";
	const char* InstancedPropertiesKey = "Properties";

	json::JSON SerializeInstancedObjectProperties(UObject* Object, const FJsonObjectReferenceContext* RefContext)
	{
		using namespace json;

		JSON Props = json::Object();
		if (!Object)
		{
			return Props;
		}

		TArray<const FProperty*> Properties;
		Object->GetClass()->GetPropertyRefs(Properties);
		for (const FProperty* Property : Properties)
		{
			if (!Property || (Property->Flags & PF_Save) == 0 || !Property->Name || Property->Name[0] == '\0')
			{
				continue;
			}

			if (!Property->GetValuePtrFor(Object))
			{
				continue;
			}

			Props[Property->Name] = Property->Serialize(Object, RefContext);
		}
		return Props;
	}

	void DeserializeInstancedObjectProperties(UObject* Object, json::JSON& Props, const FJsonObjectReferenceContext* RefContext)
	{
		if (!Object)
		{
			return;
		}

		TArray<const FProperty*> Properties;
		Object->GetClass()->GetPropertyRefs(Properties);
		for (const FProperty* Property : Properties)
		{
			if (!Property || (Property->Flags & PF_Save) == 0 || !Property->Name || Property->Name[0] == '\0')
			{
				continue;
			}

			const char* PropertyKey = Property->Name;
			if (!Props.hasKey(PropertyKey) && Property->DisplayName && Props.hasKey(Property->DisplayName))
			{
				PropertyKey = Property->DisplayName;
			}

			if (!Props.hasKey(PropertyKey) || !Property->GetValuePtrFor(Object))
			{
				continue;
			}

			json::JSON& JsonValue = Props[PropertyKey];
			Property->Deserialize(Object, JsonValue, RefContext);

			FPropertyChangedEvent Event;
			Event.Object = Object;
			Event.Property = Property;
			Event.PropertyName = Property->Name;
			Event.DisplayName = Property->DisplayName ? Property->DisplayName : Property->Name;
			Event.PropertyPath = Property->Name;
			Event.Type = Property->GetType();
			Event.ChangeType = EPropertyChangeType::Load;
			Object->PostEditChangeProperty(Event);
		}
	}
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

json::JSON FObjectProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	UObject* Object = GetObjectValueFromValuePtr(ValuePtr);
	return Object ? JSON(static_cast<int>(Object->GetUUID())) : JSON();
}

void FObjectProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	const uint32 UUID = static_cast<uint32>(Value.ToInt());
	SetObjectValueFromValuePtr(ValuePtr, UUID != 0 ? UObjectManager::Get().FindByUUID(UUID) : nullptr);
}

json::JSON FObjectProperty::SerializeValue(void* ValuePtr, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	UObject* Object = GetObjectValueFromValuePtr(ValuePtr);
	if (RefContext)
	{
		JSON RefValue;
		if (RefContext->SerializeObjectReference(Object, RefValue))
		{
			return RefValue;
		}
	}

	return SerializeValue(ValuePtr);
}

void FObjectProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, const FJsonObjectReferenceContext* RefContext) const
{
	if (RefContext)
	{
		UObject* Object = nullptr;
		if (RefContext->DeserializeObjectReference(Value, Object))
		{
			SetObjectValueFromValuePtr(ValuePtr, Object);
			return;
		}
	}

	DeserializeValue(ValuePtr, Value);
}

json::JSON FObjectProperty::Serialize(UObject* Object, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	if ((Flags & PF_InstancedReference) == 0)
	{
		return FProperty::Serialize(Object, RefContext);
	}

	UObject* InstancedObject = GetObjectValue(Object);
	if (!InstancedObject)
	{
		return JSON();
	}

	JSON Result = json::Object();
	Result[InstancedClassNameKey] = InstancedObject->GetClass()->GetName();
	Result[InstancedPropertiesKey] = SerializeInstancedObjectProperties(InstancedObject, RefContext);
	return Result;
}

void FObjectProperty::Deserialize(UObject* Object, json::JSON& Value, const FJsonObjectReferenceContext* RefContext) const
{
	if ((Flags & PF_InstancedReference) == 0)
	{
		FProperty::Deserialize(Object, Value, RefContext);
		return;
	}

	if (!Object || Value.IsNull())
	{
		SetObjectValue(Object, nullptr);
		return;
	}

	const FString ClassName = Value.hasKey(InstancedClassNameKey)
		? Value[InstancedClassNameKey].ToString()
		: FString();

	UObject* InstancedObject = GetObjectValue(Object);
	if (!ClassName.empty() &&
		(!InstancedObject || std::strcmp(InstancedObject->GetClass()->GetName(), ClassName.c_str()) != 0))
	{
		UObject* NewObject = FObjectFactory::Get().Create(ClassName, Object);
		if (NewObject)
		{
			UClass* AllowedClass = GetAllowedClassType();
			if (!AllowedClass || NewObject->GetClass()->IsA(AllowedClass))
			{
				InstancedObject = NewObject;
				SetObjectValue(Object, InstancedObject);
			}
			else
			{
				UObjectManager::Get().DestroyObject(NewObject);
			}
		}
	}
	else if (InstancedObject)
	{
		InstancedObject->SetOuter(Object);
	}

	if (InstancedObject && Value.hasKey(InstancedPropertiesKey))
	{
		DeserializeInstancedObjectProperties(InstancedObject, Value[InstancedPropertiesKey], RefContext);
	}
}

json::JSON FObjectProperty::SerializeValue(void* ValuePtr, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	using namespace json;

	(void)Owner;
	if ((Flags & PF_InstancedReference) == 0)
	{
		return SerializeValue(ValuePtr, RefContext);
	}

	UObject* InstancedObject = GetObjectValueFromValuePtr(ValuePtr);
	if (!InstancedObject)
	{
		return JSON();
	}

	JSON Result = json::Object();
	Result[InstancedClassNameKey] = InstancedObject->GetClass()->GetName();
	Result[InstancedPropertiesKey] = SerializeInstancedObjectProperties(InstancedObject, RefContext);
	return Result;
}

void FObjectProperty::DeserializeValue(void* ValuePtr, json::JSON& Value, UObject* Owner, const FJsonObjectReferenceContext* RefContext) const
{
	if ((Flags & PF_InstancedReference) == 0)
	{
		DeserializeValue(ValuePtr, Value, RefContext);
		return;
	}

	if (!ValuePtr || Value.IsNull())
	{
		SetObjectValueFromValuePtr(ValuePtr, nullptr);
		return;
	}

	const FString ClassName = Value.hasKey(InstancedClassNameKey)
		? Value[InstancedClassNameKey].ToString()
		: FString();

	UObject* InstancedObject = GetObjectValueFromValuePtr(ValuePtr);
	if (!ClassName.empty() &&
		(!InstancedObject || std::strcmp(InstancedObject->GetClass()->GetName(), ClassName.c_str()) != 0))
	{
		UObject* NewObject = FObjectFactory::Get().Create(ClassName, Owner);
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
			}
		}
	}
	else if (InstancedObject)
	{
		InstancedObject->SetOuter(Owner);
	}

	if (InstancedObject && Value.hasKey(InstancedPropertiesKey))
	{
		DeserializeInstancedObjectProperties(InstancedObject, Value[InstancedPropertiesKey], RefContext);
	}
}

void FObjectProperty::SerializeValue(void* ValuePtr, UObject* Owner, FArchive& Ar) const
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

	Ar << ClassName;

	if (Ar.IsLoading())
	{
		if (ClassName.empty() || ClassName == "None")
		{
			SetObjectValueFromValuePtr(ValuePtr, nullptr);
			return;
		}

		InstancedObject = GetObjectValueFromValuePtr(ValuePtr);
		if (!InstancedObject || std::strcmp(InstancedObject->GetClass()->GetName(), ClassName.c_str()) != 0)
		{
			UObject* NewObject = FObjectFactory::Get().Create(ClassName, Owner);
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
		else if (InstancedObject)
		{
			InstancedObject->SetOuter(Owner);
		}
	}

	if (InstancedObject)
	{
		InstancedObject->SerializeProperties(Ar, PF_Save);
	}
}

void FObjectProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	if ((Flags & PF_InstancedReference) == 0)
	{
		FProperty::Serialize(Object, Ar);
		return;
	}

	UObject* InstancedObject = nullptr;
	FString ClassName;
	if (Ar.IsSaving())
	{
		InstancedObject = GetObjectValue(Object);
		ClassName = InstancedObject ? FString(InstancedObject->GetClass()->GetName()) : FString("None");
	}

	Ar << ClassName;

	if (Ar.IsLoading())
	{
		if (ClassName.empty() || ClassName == "None")
		{
			SetObjectValue(Object, nullptr);
			return;
		}

		InstancedObject = GetObjectValue(Object);
		if (!InstancedObject || std::strcmp(InstancedObject->GetClass()->GetName(), ClassName.c_str()) != 0)
		{
			UObject* NewObject = FObjectFactory::Get().Create(ClassName, Object);
			if (NewObject)
			{
				UClass* AllowedClass = GetAllowedClassType();
				if (!AllowedClass || NewObject->GetClass()->IsA(AllowedClass))
				{
					InstancedObject = NewObject;
					SetObjectValue(Object, InstancedObject);
				}
				else
				{
					UObjectManager::Get().DestroyObject(NewObject);
					InstancedObject = nullptr;
				}
			}
		}
		else if (InstancedObject)
		{
			InstancedObject->SetOuter(Object);
		}
	}

	if (InstancedObject)
	{
		InstancedObject->SerializeProperties(Ar, PF_Save);
	}
}

void FObjectProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
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
}
