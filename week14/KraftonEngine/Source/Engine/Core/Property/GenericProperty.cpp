#include "GenericProperty.h"

#include "SimpleJSON/json.hpp"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"

json::JSON FGenericProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

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
	case EPropertyType::ByteBool:
		return JSON(static_cast<bool>(*static_cast<uint8*>(ValuePtr) != 0));
	default:
		return JSON();
	}
}

void FGenericProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
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
	default:
		break;
	}
}

void FGenericProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
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
	default:
		break;
	}
}
