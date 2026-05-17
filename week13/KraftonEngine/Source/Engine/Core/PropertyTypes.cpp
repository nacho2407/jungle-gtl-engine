#include "Core/PropertyTypes.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/FName.h"
#include "Object/UStruct.h"
#include "Serialization/Archive.h"

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

json::JSON FGenericProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr)
	{
		return JSON();
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(ValuePtr));

	case EPropertyType::Int:
		return JSON(*static_cast<int32*>(ValuePtr));

	case EPropertyType::Float:
		return JSON(static_cast<double>(*static_cast<float*>(ValuePtr)));

	case EPropertyType::Vec3:
	case EPropertyType::Rotator:
	{
		float* v = static_cast<float*>(ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Vec4:
	case EPropertyType::Color4:
	{
		float* v = static_cast<float*>(ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		arr.append(static_cast<double>(v[3]));
		return arr;
	}
	case EPropertyType::String:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::SoftObjectRef:
		return JSON(*static_cast<FString*>(ValuePtr));
	case EPropertyType::MaterialSlot:
	{
		const FMaterialSlot* Slot = static_cast<const FMaterialSlot*>(ValuePtr);
		JSON obj = json::Object();
		obj["Path"] = JSON(Slot->Path);
		return obj;
	}
	case EPropertyType::MaterialSlotArray:
	{
		const TArray<FMaterialSlot>* Slots = static_cast<const TArray<FMaterialSlot>*>(ValuePtr);
		JSON arr = json::Array();
		for (const FMaterialSlot& Slot : *Slots)
		{
			JSON obj = json::Object();
			obj["Path"] = JSON(Slot.Path);
			arr.append(obj);
		}
		return arr;
	}

	case EPropertyType::ByteBool:
		return JSON(static_cast<bool>(*static_cast<uint8_t*>(ValuePtr) != 0));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(ValuePtr)->ToString());

	case EPropertyType::Vec3Array:
	{
		const TArray<FVector>* Arr = static_cast<const TArray<FVector>*>(ValuePtr);
		JSON outer = json::Array();
		for (const FVector& v : *Arr)
		{
			JSON inner = json::Array();
			inner.append(static_cast<double>(v.X));
			inner.append(static_cast<double>(v.Y));
			inner.append(static_cast<double>(v.Z));
			outer.append(inner);
		}
		return outer;
	}

	case EPropertyType::Struct:
	{
		JSON obj = json::Object();
		if (!StructType)
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

	default:
		return JSON();
	}
}

void FGenericProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		*static_cast<bool*>(ValuePtr) = Value.ToBool();
		break;

	case EPropertyType::ByteBool:
		*static_cast<uint8_t*>(ValuePtr) = Value.ToBool() ? 1 : 0;
		break;

	case EPropertyType::Int:
		*static_cast<int32*>(ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Float:
		*static_cast<float*>(ValuePtr) = static_cast<float>(Value.ToFloat());
		break;

	case EPropertyType::Vec3:
	case EPropertyType::Rotator:
	{
		float* v = static_cast<float*>(ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange())
		{
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			++i;
		}
		break;
	}
	case EPropertyType::Vec4:
	case EPropertyType::Color4:
	{
		float* v = static_cast<float*>(ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange())
		{
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			++i;
		}
		break;
	}
	case EPropertyType::String:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::SoftObjectRef:
		*static_cast<FString*>(ValuePtr) = Value.ToString();
		break;

	case EPropertyType::MaterialSlot:
	{
		FMaterialSlot* Slot = static_cast<FMaterialSlot*>(ValuePtr);
		if (Value.hasKey("Path")) Slot->Path = Value["Path"].ToString();
		break;
	}
	case EPropertyType::MaterialSlotArray:
	{
		TArray<FMaterialSlot>* Slots = static_cast<TArray<FMaterialSlot>*>(ValuePtr);
		TArray<FMaterialSlot> LoadedSlots;
		for (auto& elem : Value.ArrayRange())
		{
			FMaterialSlot Slot;
			if (elem.hasKey("Path")) Slot.Path = elem["Path"].ToString();
			LoadedSlots.push_back(Slot);
		}
		*Slots = LoadedSlots;
		break;
	}

	case EPropertyType::Name:
		*static_cast<FName*>(ValuePtr) = FName(Value.ToString());
		break;

	case EPropertyType::Vec3Array:
	{
		TArray<FVector>* Arr = static_cast<TArray<FVector>*>(ValuePtr);
		Arr->clear();
		for (auto& elem : Value.ArrayRange())
		{
			FVector v(0, 0, 0);
			int i = 0;
			for (auto& c : elem.ArrayRange())
			{
				if (i == 0) v.X = static_cast<float>(c.ToFloat());
				else if (i == 1) v.Y = static_cast<float>(c.ToFloat());
				else if (i == 2) v.Z = static_cast<float>(c.ToFloat());
				++i;
			}
			Arr->push_back(v);
		}
		break;
	}

	case EPropertyType::Struct:
	{
		if (!StructType)
		{
			break;
		}

		TArray<const FProperty*> Children;
		StructType->GetPropertyRefs(Children);
		for (const FProperty* Child : Children)
		{
			if (!Child || !Child->Name || !Value.hasKey(Child->Name)) continue;
			json::JSON& ChildVal = Value[Child->Name];
			Child->Deserialize(ValuePtr, ChildVal);
		}
		break;
	}

	default:
		break;
	}
}

void FGenericProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		Ar << *static_cast<bool*>(ValuePtr);
		break;
	case EPropertyType::ByteBool:
		Ar << *static_cast<uint8*>(ValuePtr);
		break;
	case EPropertyType::Int:
		Ar << *static_cast<int32*>(ValuePtr);
		break;
	case EPropertyType::Float:
		Ar << *static_cast<float*>(ValuePtr);
		break;
	case EPropertyType::Vec3:
		Ar << *static_cast<FVector*>(ValuePtr);
		break;
	case EPropertyType::Rotator:
		Ar << *static_cast<FRotator*>(ValuePtr);
		break;
	case EPropertyType::Vec4:
	case EPropertyType::Color4:
		Ar << *static_cast<FVector4*>(ValuePtr);
		break;
	case EPropertyType::String:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::SoftObjectRef:
		Ar << *static_cast<FString*>(ValuePtr);
		break;
	case EPropertyType::MaterialSlot:
		Ar << static_cast<FMaterialSlot*>(ValuePtr)->Path;
		break;
	case EPropertyType::MaterialSlotArray:
	{
		TArray<FMaterialSlot>* Slots = static_cast<TArray<FMaterialSlot>*>(ValuePtr);
		uint32 SlotCount = static_cast<uint32>(Slots->size());
		Ar << SlotCount;
		if (Ar.IsLoading())
		{
			Slots->resize(SlotCount);
		}
		for (FMaterialSlot& Slot : *Slots)
		{
			Ar << Slot.Path;
		}
		break;
	}
	case EPropertyType::Name:
		Ar << *static_cast<FName*>(ValuePtr);
		break;
	case EPropertyType::Vec3Array:
		Ar << *static_cast<TArray<FVector>*>(ValuePtr);
		break;
	case EPropertyType::Struct:
	{
		if (!StructType)
		{
			break;
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
		break;
	}
	default:
		break;
	}
}

json::JSON FEnumProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr)
	{
		return JSON();
	}

	const uint32 ResolvedEnumSize = EnumType ? EnumType->GetSize() : sizeof(int32);
	int32 Val = 0;
	std::memcpy(&Val, ValuePtr, ResolvedEnumSize);
	return JSON(Val);
}

void FEnumProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr)
	{
		return;
	}

	const uint32 ResolvedEnumSize = EnumType ? EnumType->GetSize() : sizeof(int32);
	int32 Val = Value.ToInt();
	std::memcpy(ValuePtr, &Val, ResolvedEnumSize);
}

void FEnumProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr)
	{
		return;
	}

	Ar.Serialize(ValuePtr, EnumType ? EnumType->GetSize() : sizeof(int32));
}

json::JSON FProperty::Serialize(UObject* Object) const
{
	return Serialize(static_cast<void*>(Object));
}

void FProperty::Deserialize(UObject* Object, json::JSON& JsonValue) const
{
	Deserialize(static_cast<void*>(Object), JsonValue);
}

void FProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	Serialize(static_cast<void*>(Object), Ar);
}
