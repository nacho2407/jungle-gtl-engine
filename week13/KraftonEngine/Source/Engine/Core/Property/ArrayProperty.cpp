#include "ArrayProperty.h"

#include "SimpleJSON/json.hpp"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"

namespace
{
	json::JSON SerializeElement(EPropertyType ElementType, const FArrayProperty::FElementPathOps* PathOps, const void* ElementPtr)
	{
		using namespace json;

		if (!ElementPtr)
		{
			return JSON();
		}

		switch (ElementType)
		{
		case EPropertyType::Bool:
			return JSON(*static_cast<const bool*>(ElementPtr));
		case EPropertyType::ByteBool:
			return JSON(static_cast<bool>(*static_cast<const uint8*>(ElementPtr) != 0));
		case EPropertyType::Int:
			return JSON(*static_cast<const int32*>(ElementPtr));
		case EPropertyType::Float:
			return JSON(static_cast<double>(*static_cast<const float*>(ElementPtr)));
		case EPropertyType::String:
		case EPropertyType::SceneComponentRef:
			return JSON(*static_cast<const FString*>(ElementPtr));
		case EPropertyType::SoftObjectRef:
			return JSON(PathOps && PathOps->GetPath
				? PathOps->GetPath(ElementPtr)
				: *static_cast<const FString*>(ElementPtr));
		case EPropertyType::Name:
			return JSON(static_cast<const FName*>(ElementPtr)->ToString());
		case EPropertyType::Vec3:
		case EPropertyType::Rotator:
		{
			const float* v = static_cast<const float*>(ElementPtr);
			JSON arr = json::Array();
			arr.append(static_cast<double>(v[0]));
			arr.append(static_cast<double>(v[1]));
			arr.append(static_cast<double>(v[2]));
			return arr;
		}
		case EPropertyType::Vec4:
		case EPropertyType::Color4:
		{
			const float* v = static_cast<const float*>(ElementPtr);
			JSON arr = json::Array();
			arr.append(static_cast<double>(v[0]));
			arr.append(static_cast<double>(v[1]));
			arr.append(static_cast<double>(v[2]));
			arr.append(static_cast<double>(v[3]));
			return arr;
		}
		default:
			return JSON();
		}
	}

	void DeserializeElement(EPropertyType ElementType, const FArrayProperty::FElementPathOps* PathOps, void* ElementPtr, json::JSON& Value)
	{
		if (!ElementPtr)
		{
			return;
		}

		switch (ElementType)
		{
		case EPropertyType::Bool:
			*static_cast<bool*>(ElementPtr) = Value.ToBool();
			break;
		case EPropertyType::ByteBool:
			*static_cast<uint8*>(ElementPtr) = Value.ToBool() ? 1 : 0;
			break;
		case EPropertyType::Int:
			*static_cast<int32*>(ElementPtr) = Value.ToInt();
			break;
		case EPropertyType::Float:
			*static_cast<float*>(ElementPtr) = static_cast<float>(Value.ToFloat());
			break;
		case EPropertyType::String:
		case EPropertyType::SceneComponentRef:
			*static_cast<FString*>(ElementPtr) = Value.ToString();
			break;
		case EPropertyType::SoftObjectRef:
			if (PathOps && PathOps->SetPath)
			{
				PathOps->SetPath(ElementPtr, Value.ToString());
			}
			else
			{
				*static_cast<FString*>(ElementPtr) = Value.ToString();
			}
			break;
		case EPropertyType::Name:
			*static_cast<FName*>(ElementPtr) = FName(Value.ToString());
			break;
		case EPropertyType::Vec3:
		case EPropertyType::Rotator:
		{
			float* v = static_cast<float*>(ElementPtr);
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
			float* v = static_cast<float*>(ElementPtr);
			int i = 0;
			for (auto& elem : Value.ArrayRange())
			{
				if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
				++i;
			}
			break;
		}
		default:
			break;
		}
	}

	void SerializeElement(FArchive& Ar, EPropertyType ElementType, const FArrayProperty::FElementPathOps* PathOps, void* ElementPtr)
	{
		if (!ElementPtr)
		{
			return;
		}

		switch (ElementType)
		{
		case EPropertyType::Bool:
			Ar << *static_cast<bool*>(ElementPtr);
			break;
		case EPropertyType::ByteBool:
			Ar << *static_cast<uint8*>(ElementPtr);
			break;
		case EPropertyType::Int:
			Ar << *static_cast<int32*>(ElementPtr);
			break;
		case EPropertyType::Float:
			Ar << *static_cast<float*>(ElementPtr);
			break;
		case EPropertyType::String:
		case EPropertyType::SceneComponentRef:
			Ar << *static_cast<FString*>(ElementPtr);
			break;
		case EPropertyType::SoftObjectRef:
			if (PathOps && PathOps->SerializeArchive)
			{
				PathOps->SerializeArchive(ElementPtr, Ar);
			}
			else
			{
				Ar << *static_cast<FString*>(ElementPtr);
			}
			break;
		case EPropertyType::Name:
			Ar << *static_cast<FName*>(ElementPtr);
			break;
		case EPropertyType::Vec3:
			Ar << *static_cast<FVector*>(ElementPtr);
			break;
		case EPropertyType::Rotator:
			Ar << *static_cast<FRotator*>(ElementPtr);
			break;
		case EPropertyType::Vec4:
		case EPropertyType::Color4:
			Ar << *static_cast<FVector4*>(ElementPtr);
			break;
		default:
			break;
		}
	}
}

json::JSON FArrayProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr || !Ops || !Ops->GetNum || !Ops->GetConstElementPtr)
	{
		return JSON();
	}

	JSON arr = json::Array();
	const size_t Num = Ops->GetNum(ValuePtr);
	for (size_t Index = 0; Index < Num; ++Index)
	{
		arr.append(SerializeElement(ElementType, ElementPathOps, Ops->GetConstElementPtr(ValuePtr, Index)));
	}
	return arr;
}

void FArrayProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr || !Ops || !Ops->Resize || !Ops->GetElementPtr)
	{
		return;
	}

	size_t Num = 0;
	for (auto& elem : Value.ArrayRange())
	{
		(void)elem;
		++Num;
	}

	Ops->Resize(ValuePtr, Num);

	size_t Index = 0;
	for (auto& elem : Value.ArrayRange())
	{
		DeserializeElement(ElementType, ElementPathOps, Ops->GetElementPtr(ValuePtr, Index), elem);
		++Index;
	}
}

void FArrayProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (!ValuePtr || !Ops || !Ops->GetNum || !Ops->Resize || !Ops->GetElementPtr)
	{
		return;
	}

	uint32 Num = static_cast<uint32>(Ops->GetNum(ValuePtr));
	Ar << Num;
	if (Ar.IsLoading())
	{
		Ops->Resize(ValuePtr, Num);
	}

	for (uint32 Index = 0; Index < Num; ++Index)
	{
		SerializeElement(Ar, ElementType, ElementPathOps, Ops->GetElementPtr(ValuePtr, Index));
	}
}
