#include "EnumProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Serialization/Archive.h"

json::JSON FEnumProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	if (!ValuePtr)
	{
		return JSON();
	}

	const uint32 ResolvedEnumSize = EnumType ? EnumType->GetSize() : sizeof(int32);
	int32 Val = 0;
	std::memcpy(&Val, ValuePtr, ResolvedEnumSize);
	return JSON(Val);
}

void FEnumProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (!ValuePtr)
	{
		return;
	}

	const uint32 ResolvedEnumSize = EnumType ? EnumType->GetSize() : sizeof(int32);
	int32 Val = Value.ToInt();
	std::memcpy(ValuePtr, &Val, ResolvedEnumSize);
}

void FEnumProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr)
	{
		return;
	}

	Ar.Serialize(ValuePtr, EnumType ? EnumType->GetSize() : sizeof(int32));
}
