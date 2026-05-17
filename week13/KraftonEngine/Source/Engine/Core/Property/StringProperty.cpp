#include "StringProperty.h"

#include "SimpleJSON/json.hpp"
#include "Serialization/Archive.h"

json::JSON FStringProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	return ValuePtr ? JSON(*static_cast<FString*>(ValuePtr)) : JSON();
}

void FStringProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (ValuePtr)
	{
		*static_cast<FString*>(ValuePtr) = Value.ToString();
	}
}

void FStringProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<FString*>(ValuePtr);
	}
}
