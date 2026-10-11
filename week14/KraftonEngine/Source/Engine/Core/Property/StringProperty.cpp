#include "StringProperty.h"

#include "Serialization/Archive.h"

void FStringProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<FString*>(ValuePtr);
	}
}
