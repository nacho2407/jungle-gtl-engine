#include "DistributionFloat.h"
#include "Math/RandomStream.h"
#include "Serialization/Archive.h"
#include <cstdlib>

float UDistributionFloatUniform::GetValue(float Time, UObject* Data, FRandomStream* InRandomStream) const
{
	float Alpha = (InRandomStream ? InRandomStream->FRand() : (float)rand() / (float)RAND_MAX);
	return FMath::Lerp(Min, Max, Alpha);
}

float FRawDistributionFloat::GetValue(float Time, UObject* Data, FRandomStream* InRandomStream) const
{
	if (IsSimple())
	{
		float Result;
		GetValue1None(Time, &Result);
		return Result;
	}
	else if (Distribution)
	{
		return Distribution->GetValue(Time, Data, InRandomStream);
	}
	return 0.0f;
}

bool FRawDistributionFloat::Serialize(FArchive& Ar)
{
	FRawDistribution::Serialize(Ar);
	Ar << MinValue;
	Ar << MaxValue;

	UObject* Obj = Distribution;
	Ar.SerializeObjectReference(Obj);
	if (Ar.IsLoading())
	{
		Distribution = Cast<UDistributionFloat>(Obj);
	}
	return true;
}
