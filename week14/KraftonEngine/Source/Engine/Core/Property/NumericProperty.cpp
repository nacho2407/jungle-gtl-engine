#include "NumericProperty.h"

#include <cstring>
#include "Core/Types/CoreTypes.h"
#include "Serialization/Archive.h"

void FIntProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<int32*>(ValuePtr);
	}
}

void FFloatProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (ValuePtr)
	{
		Ar << *static_cast<float*>(ValuePtr);
	}
}
