#include "NumericProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

json::JSON FIntProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	return ValuePtr ? JSON(*static_cast<int32*>(ValuePtr)) : JSON();
}

void FIntProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (ValuePtr)
	{
		*static_cast<int32*>(ValuePtr) = Value.ToInt();
	}
}

void FIntProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<int32*>(ValuePtr);
	}
}

json::JSON FFloatProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	return ValuePtr ? JSON(static_cast<double>(*static_cast<float*>(ValuePtr))) : JSON();
}

void FFloatProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (ValuePtr)
	{
		*static_cast<float*>(ValuePtr) = static_cast<float>(Value.ToFloat());
	}
}

void FFloatProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<float*>(ValuePtr);
	}
}
