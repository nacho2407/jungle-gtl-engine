#include "EnumProperty.h"

#include <cstring>
#include "Serialization/Archive.h"

void FEnumProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr)
	{
		return;
	}

	const uint32 ResolvedEnumSize = EnumType ? EnumType->GetSize() : sizeof(int32);
	int32 Val = 0;
	if (Ar.IsSaving())
	{
		std::memcpy(&Val, ValuePtr, ResolvedEnumSize);
	}

	Ar << Val;

	if (Ar.IsLoading())
	{
		std::memcpy(ValuePtr, &Val, ResolvedEnumSize);
	}
}
