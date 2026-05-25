#include "DistributionFloat.h"
#include "Math/RandomStream.h"
#include "Serialization/Archive.h"
#include "Object/Reflection/ObjectFactory.h"
#include <cstdlib>

void UDistributionFloat::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
}

void UDistributionFloatConstant::Serialize(FArchive& Ar)
{
	UDistributionFloat::Serialize(Ar);
	Ar << Constant;
}

void UDistributionFloatUniform::Serialize(FArchive& Ar)
{
	UDistributionFloat::Serialize(Ar);
	Ar << Min;
	Ar << Max;
}

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

	FString ClassName = (Ar.IsSaving() && Distribution)
		? FString(Distribution->GetClass()->GetName())
		: FString("None");
	Ar << ClassName;

	if (Ar.IsLoading())
	{
		Distribution = nullptr;
		if (!ClassName.empty() && ClassName != "None")
		{
			UObject* Created = FObjectFactory::Get().Create(ClassName, Ar.IsSaving() ? nullptr : nullptr); // Outer will be set by caller or during create
			Distribution = Cast<UDistributionFloat>(Created);
		}
	}

	if (Distribution)
	{
		Distribution->Serialize(Ar);
	}

	return true;
}
