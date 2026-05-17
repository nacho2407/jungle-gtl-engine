#include "StringProperty.h"

#include "SimpleJSON/json.hpp"
#include "Serialization/Archive.h"

json::JSON FStringProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr ? JSON(*static_cast<FString*>(ValuePtr)) : JSON();
}

void FStringProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		*static_cast<FString*>(ValuePtr) = Value.ToString();
	}
}

void FStringProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		Ar << *static_cast<FString*>(ValuePtr);
	}
}
