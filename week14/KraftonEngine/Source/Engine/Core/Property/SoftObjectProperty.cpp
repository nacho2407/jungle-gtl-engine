#include "SoftObjectProperty.h"

#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

json::JSON FSoftObjectProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr ? JSON(*static_cast<FString*>(ValuePtr)) : JSON();
}

void FSoftObjectProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		*static_cast<FString*>(ValuePtr) = Value.ToString();
	}
}

void FSoftObjectProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		Ar << *static_cast<FString*>(ValuePtr);
	}
}
