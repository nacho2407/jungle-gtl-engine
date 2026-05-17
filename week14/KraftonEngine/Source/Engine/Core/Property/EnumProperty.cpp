#include "EnumProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Serialization/Archive.h"

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
