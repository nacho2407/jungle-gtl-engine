#include "Core/PropertyTypes.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"

json::JSON FPropertyValue::Serialize() const
{
	using namespace json;

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
	case EPropertyType::Script:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
		return JSON(*static_cast<FString*>(ValuePtr));
	case EPropertyType::SkeletalMeshRef:
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

	case EPropertyType::Enum:
	{
		int32 Val = 0;
		std::memcpy(&Val, ValuePtr, EnumSize);
		return JSON(Val);
	}

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
		if (!StructFunc || !ValuePtr) return JSON();
		TArray<FPropertyValue> Children;
		StructFunc(ValuePtr, Children);
		JSON obj = json::Object();
		for (const auto& Child : Children)
		{
			obj[Child.Name] = Child.Serialize();
		}
		return obj;
	}

	default:
		return JSON();
	}
}

void FPropertyValue::Deserialize(json::JSON& Value)
{
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
	case EPropertyType::Script:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
		*static_cast<FString*>(ValuePtr) = Value.ToString();
		break;
	case EPropertyType::SkeletalMeshRef:
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

	case EPropertyType::Enum:
	{
		int32 Val = Value.ToInt();
		std::memcpy(ValuePtr, &Val, EnumSize);
		break;
	}

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
		if (!StructFunc || !ValuePtr) break;
		TArray<FPropertyValue> Children;
		StructFunc(ValuePtr, Children);
		for (auto& Child : Children)
		{
			if (!Value.hasKey(Child.Name.c_str())) continue;
			json::JSON& ChildVal = Value[Child.Name.c_str()];
			Child.Deserialize(ChildVal);
		}
		break;
	}

	default:
		break;
	}
}

void FPropertyValue::Serialize(FArchive& Ar) const
{
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
	case EPropertyType::Script:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
	case EPropertyType::SkeletalMeshRef:
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
	case EPropertyType::Enum:
		Ar.Serialize(ValuePtr, EnumSize);
		break;
	case EPropertyType::Vec3Array:
		Ar << *static_cast<TArray<FVector>*>(ValuePtr);
		break;
	case EPropertyType::Struct:
	{
		if (!StructFunc || !ValuePtr) break;
		TArray<FPropertyValue> Children;
		StructFunc(ValuePtr, Children);
		for (const FPropertyValue& Child : Children)
		{
			Child.Serialize(Ar);
		}
		break;
	}
	default:
		break;
	}
}

json::JSON FProperty::Serialize(UObject* Object) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Object);
	if (!ValuePtr)
	{
		return JSON();
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(ValuePtr));
	case EPropertyType::ByteBool:
		return JSON(static_cast<bool>(*static_cast<uint8_t*>(ValuePtr) != 0));
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
	case EPropertyType::Script:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
	case EPropertyType::SkeletalMeshRef:
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
	case EPropertyType::Name:
		return JSON(static_cast<FName*>(ValuePtr)->ToString());
	case EPropertyType::Enum:
	{
		int32 Val = 0;
		std::memcpy(&Val, ValuePtr, EnumSize);
		return JSON(Val);
	}
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
		if (!StructFunc) return JSON();
		TArray<FPropertyValue> Children;
		StructFunc(ValuePtr, Children);
		JSON obj = json::Object();
		for (const auto& Child : Children)
		{
			obj[Child.Name] = Child.Serialize();
		}
		return obj;
	}
	default:
		return JSON();
	}
}

void FProperty::Deserialize(UObject* Object, json::JSON& JsonValue) const
{
	void* ValuePtr = GetValuePtrFor(Object);
	if (!ValuePtr)
	{
		return;
	}

	switch (Type)
	{
	case EPropertyType::Bool:
		*static_cast<bool*>(ValuePtr) = JsonValue.ToBool();
		break;
	case EPropertyType::ByteBool:
		*static_cast<uint8_t*>(ValuePtr) = JsonValue.ToBool() ? 1 : 0;
		break;
	case EPropertyType::Int:
		*static_cast<int32*>(ValuePtr) = JsonValue.ToInt();
		break;
	case EPropertyType::Float:
		*static_cast<float*>(ValuePtr) = static_cast<float>(JsonValue.ToFloat());
		break;
	case EPropertyType::Vec3:
	case EPropertyType::Rotator:
	{
		float* v = static_cast<float*>(ValuePtr);
		int i = 0;
		for (auto& elem : JsonValue.ArrayRange())
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
		for (auto& elem : JsonValue.ArrayRange())
		{
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			++i;
		}
		break;
	}
	case EPropertyType::String:
	case EPropertyType::Script:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
	case EPropertyType::SkeletalMeshRef:
		*static_cast<FString*>(ValuePtr) = JsonValue.ToString();
		break;
	case EPropertyType::MaterialSlot:
	{
		FMaterialSlot* Slot = static_cast<FMaterialSlot*>(ValuePtr);
		if (JsonValue.hasKey("Path")) Slot->Path = JsonValue["Path"].ToString();
		break;
	}
	case EPropertyType::MaterialSlotArray:
	{
		TArray<FMaterialSlot>* Slots = static_cast<TArray<FMaterialSlot>*>(ValuePtr);
		TArray<FMaterialSlot> LoadedSlots;
		for (auto& elem : JsonValue.ArrayRange())
		{
			FMaterialSlot Slot;
			if (elem.hasKey("Path")) Slot.Path = elem["Path"].ToString();
			LoadedSlots.push_back(Slot);
		}
		*Slots = LoadedSlots;
		break;
	}
	case EPropertyType::Name:
		*static_cast<FName*>(ValuePtr) = FName(JsonValue.ToString());
		break;
	case EPropertyType::Enum:
	{
		int32 Val = JsonValue.ToInt();
		std::memcpy(ValuePtr, &Val, EnumSize);
		break;
	}
	case EPropertyType::Vec3Array:
	{
		TArray<FVector>* Arr = static_cast<TArray<FVector>*>(ValuePtr);
		Arr->clear();
		for (auto& elem : JsonValue.ArrayRange())
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
		if (!StructFunc) break;
		TArray<FPropertyValue> Children;
		StructFunc(ValuePtr, Children);
		for (auto& Child : Children)
		{
			if (!JsonValue.hasKey(Child.Name.c_str())) continue;
			json::JSON& ChildVal = JsonValue[Child.Name.c_str()];
			Child.Deserialize(ChildVal);
		}
		break;
	}
	default:
		break;
	}
}

void FProperty::Serialize(UObject* Object, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Object);
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
	case EPropertyType::Script:
	case EPropertyType::SceneComponentRef:
	case EPropertyType::StaticMeshRef:
	case EPropertyType::SkeletalMeshRef:
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
	case EPropertyType::Enum:
		Ar.Serialize(ValuePtr, EnumSize);
		break;
	case EPropertyType::Vec3Array:
		Ar << *static_cast<TArray<FVector>*>(ValuePtr);
		break;
	case EPropertyType::Struct:
	{
		if (!StructFunc) break;
		TArray<FPropertyValue> Children;
		StructFunc(ValuePtr, Children);
		for (const FPropertyValue& Child : Children)
		{
			Child.Serialize(Ar);
		}
		break;
	}
	default:
		break;
	}
}
