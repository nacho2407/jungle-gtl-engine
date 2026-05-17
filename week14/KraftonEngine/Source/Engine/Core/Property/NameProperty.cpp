#include "NameProperty.h"

#include "Object/FName.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

json::JSON FNameProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	return ValuePtr ? JSON(static_cast<FName*>(ValuePtr)->ToString()) : JSON();
}

void FNameProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (ValuePtr)
	{
		*static_cast<FName*>(ValuePtr) = FName(Value.ToString());
	}
}

void FNameProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<FName*>(ValuePtr);
	}
}
