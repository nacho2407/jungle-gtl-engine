#include "SoftObjectProperty.h"

#include "Serialization/Archive.h"
#include "SimpleJSON/json.hpp"

json::JSON FSoftObjectProperty::Serialize(void* Container) const
{
	using namespace json;

	void* ValuePtr = GetValuePtrFor(Container);
	return ValuePtr && Ops && Ops->GetPath ? JSON(Ops->GetPath(ValuePtr)) : JSON();
}

void FSoftObjectProperty::Deserialize(void* Container, json::JSON& Value) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr && Ops && Ops->SetPath)
	{
		Ops->SetPath(ValuePtr, Value.ToString());
	}
}

void FSoftObjectProperty::Serialize(void* Container, FArchive& Ar) const
{
	void* ValuePtr = GetValuePtrFor(Container);
	if (ValuePtr && Ops && Ops->SerializeArchive)
	{
		Ops->SerializeArchive(ValuePtr, Ar);
	}
}
