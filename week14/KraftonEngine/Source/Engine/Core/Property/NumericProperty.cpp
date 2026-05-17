#include "NumericProperty.h"

#include <cstring>
#include "SimpleJSON/json.hpp"
#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

json::JSON FIntProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr ? JSON(*static_cast<int32*>(ValuePtr)) : JSON();
}

void FIntProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		*static_cast<int32*>(ValuePtr) = Value.ToInt();
	}
}

void FIntProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		Ar << *static_cast<int32*>(ValuePtr);
	}
}

json::JSON FFloatProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr ? JSON(static_cast<double>(*static_cast<float*>(ValuePtr))) : JSON();
}

void FFloatProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		*static_cast<float*>(ValuePtr) = static_cast<float>(Value.ToFloat());
	}
}

void FFloatProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr)
	{
		Ar << *static_cast<float*>(ValuePtr);
	}
}