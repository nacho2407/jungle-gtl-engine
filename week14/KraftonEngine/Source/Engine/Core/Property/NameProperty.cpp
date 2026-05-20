#include "NameProperty.h"

#include "Object/FName.h"
#include "Serialization/Archive.h"

void FNameProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<FName*>(ValuePtr);
	}
}
