#include "BoolProperty.h"

#include "SimpleJSON/json.hpp"
#include "Serialization/Archive.h"

json::JSON FBoolProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr ? JSON(*static_cast<bool*>(ValuePtr)) : JSON();
}

void FBoolProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		*static_cast<bool*>(ValuePtr) = Value.ToBool();
	}
}

void FBoolProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		Ar << *static_cast<bool*>(ValuePtr);
	}
}
