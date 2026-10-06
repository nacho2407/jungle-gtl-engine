#include "BoolProperty.h"

#include "Serialization/Archive.h"

void FBoolProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<bool*>(ValuePtr);
	}
}
