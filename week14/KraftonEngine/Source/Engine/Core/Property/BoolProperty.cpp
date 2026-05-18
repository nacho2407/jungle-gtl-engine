#include "BoolProperty.h"

#include "SimpleJSON/json.hpp"
#include "Serialization/Archive.h"

json::JSON FBoolProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	return ValuePtr ? JSON(*static_cast<bool*>(ValuePtr)) : JSON();
}

void FBoolProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (ValuePtr)
	{
		*static_cast<bool*>(ValuePtr) = Value.ToBool();
	}
}

void FBoolProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<bool*>(ValuePtr);
	}
}
