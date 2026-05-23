#include "DistributionVector.h"
#include "Math/RandomStream.h"
#include <cstdlib>

FVector UDistributionVectorUniform::GetValue(float Time, UObject* Data, FRandomStream* InRandomStream) const
{
	float AlphaX = (InRandomStream ? InRandomStream->FRand() : (float)rand() / (float)RAND_MAX);
	float AlphaY = (InRandomStream ? InRandomStream->FRand() : (float)rand() / (float)RAND_MAX);
	float AlphaZ = (InRandomStream ? InRandomStream->FRand() : (float)rand() / (float)RAND_MAX);

	if (bLockAxes)
	{
		AlphaY = AlphaX;
		AlphaZ = AlphaX;
	}

	FVector Result;
	Result.X = FMath::Lerp(Min.X, Max.X, AlphaX);
	Result.Y = FMath::Lerp(Min.Y, Max.Y, AlphaY);
	Result.Z = FMath::Lerp(Min.Z, Max.Z, AlphaZ);
	return Result;
}

FVector FRawDistributionVector::GetValue(float Time, UObject* Data, FRandomStream* InRandomStream) const
{
	if (IsSimple())
	{
		FVector Result;
		GetValue3None(Time, &Result.X);
		return Result;
	}
	else if (Distribution)
	{
		return Distribution->GetValue(Time, Data, InRandomStream);
	}
	return FVector::ZeroVector;
}
