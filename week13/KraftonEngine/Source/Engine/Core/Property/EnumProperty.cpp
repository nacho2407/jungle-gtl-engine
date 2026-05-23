#include "EnumProperty.h"

#include <algorithm>
#include <cstring>
#include "Serialization/Archive.h"

void FEnumProperty::SerializeValue(void* ValuePtr, FArchive& Ar) const
{
	if (!ValuePtr)
	{
		return;
	}

	const uint32 ResolvedEnumSize = EnumType ? EnumType->GetSize() : sizeof(int32);
	int64 Val = 0;
	if (Ar.IsSaving())
	{
		std::memcpy(&Val, ValuePtr, std::min<size_t>(ResolvedEnumSize, sizeof(Val)));
	}

	Ar << Val;

	if (Ar.IsLoading())
	{
		std::memcpy(ValuePtr, &Val, std::min<size_t>(ResolvedEnumSize, sizeof(Val)));
	}
}
