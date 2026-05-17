#include "NameProperty.h"

#include "Object/FName.h"
#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

json::JSON FNameProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr ? JSON(static_cast<FName*>(ValuePtr)->ToString()) : JSON();
}

void FNameProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		*static_cast<FName*>(ValuePtr) = FName(Value.ToString());
	}
}

void FNameProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		Ar << *static_cast<FName*>(ValuePtr);
	}
}
