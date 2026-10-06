#include "Distributions.h"
#include "Serialization/Archive.h"
#include "Object/Object.h"

bool FRawDistribution::Serialize(FArchive& Ar)
{
	Ar << LookupTable.TimeScale;
	Ar << LookupTable.TimeBias;
	Ar << LookupTable.Values;
	Ar << LookupTable.Op;
	Ar << LookupTable.EntryCount;
	Ar << LookupTable.EntryStride;
	Ar << LookupTable.SubEntryStride;
	Ar << LookupTable.LockFlag;
	return true;
}

void FRawDistribution::GetValue(float Time, float* Value, int32 NumCoords, int32 Extreme, FRandomStream* InRandomStream) const
{
	// checkSlow(NumCoords == 3 || NumCoords == 1); 같이 언리얼에는 Coord 개수를 1 또는 3으로 제한하는 assert 문이 있는데
	// 당장은 넣지 않았음
	switch (LookupTable.Op)
	{
	case RDO_None:
		if (NumCoords == 1)
		{
			GetValue1None(Time, Value);
		}
		else
		{
			GetValue3None(Time, Value);
		}
		break;
	}
}

void FRawDistribution::GetValue1(float Time, float* Value, int32 Extreme, FRandomStream* InRandomStream) const
{
	switch (LookupTable.Op)
	{
	case RDO_None:
		GetValue1None(Time, Value);
		break;
	default: // compiler complains
		*Value = 0.0f;
		break;
	}
}

void FRawDistribution::GetValue3(float Time, float* Value, int32 Extreme, FRandomStream* InRandomStream) const
{
	switch (LookupTable.Op)
	{
	case RDO_None:
		GetValue3None(Time, Value);
		break;
	}
}
