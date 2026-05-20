#include "SoftObjectProperty.h"

#include "Serialization/Archive.h"

void FSoftObjectProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr && Ops && Ops->SerializeArchive)
	{
		Ops->SerializeArchive(ValuePtr, Ar);
	}
}
