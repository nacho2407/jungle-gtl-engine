#include "GenericProperty.h"

#include "SimpleJSON/json.hpp"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"

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
		return JSON(static_cast<bool>(*static_cast<uint8*>(ValuePtr) != 0));
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
	case EPropertyType::ByteBool:
		*static_cast<uint8*>(ValuePtr) = Value.ToBool() ? 1 : 0;
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
	case EPropertyType::ByteBool:
		Ar << *static_cast<uint8*>(ValuePtr);
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
	default:
		break;
	}
}
