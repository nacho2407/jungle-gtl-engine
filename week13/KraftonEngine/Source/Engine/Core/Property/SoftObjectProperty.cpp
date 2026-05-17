#include "SoftObjectProperty.h"

#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

json::JSON FSoftObjectProperty::SerializeValue(void* ValuePtr) const
{
	using namespace json;

	return ValuePtr && Ops && Ops->GetPath ? JSON(Ops->GetPath(ValuePtr)) : JSON();
}

void FSoftObjectProperty::DeserializeValue(void* ValuePtr, json::JSON& Value) const
{
	if (ValuePtr && Ops && Ops->SetPath)
	{
		Ops->SetPath(ValuePtr, Value.ToString());
	}
}

void FSoftObjectProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr && Ops && Ops->SerializeArchive)
	{
		Ops->SerializeArchive(ValuePtr, Ar);
	}
}
