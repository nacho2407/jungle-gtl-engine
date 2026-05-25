#include "DistributionVector.h"
#include "Math/RandomStream.h"
#include "Serialization/Archive.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/GarbageCollection.h"
#include <cstdlib>

void UDistributionVector::Serialize(FArchive& Ar)
{
	UObject::Serialize(Ar);
}

void UDistributionVectorConstant::Serialize(FArchive& Ar)
{
	UDistributionVector::Serialize(Ar);
	Ar << Constant;
}

void UDistributionVectorUniform::Serialize(FArchive& Ar)
{
	UDistributionVector::Serialize(Ar);
	Ar << Min;
	Ar << Max;
	Ar << bLockAxes;
}

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


void FRawDistributionVector::AddReferencedObjects(FReferenceCollector& Collector) const
{
	Collector.AddReferencedObject(Distribution, "FRawDistributionVector.Distribution");
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

bool FRawDistributionVector::Serialize(FArchive& Ar)
{
	FRawDistribution::Serialize(Ar);
	Ar << MinValue;
	Ar << MaxValue;
	Ar << MinValueVec;
	Ar << MaxValueVec;

	FString ClassName = (Ar.IsSaving() && Distribution)
		? FString(Distribution->GetClass()->GetName())
		: FString("None");
	Ar << ClassName;

	if (Ar.IsLoading())
	{
		Distribution = nullptr;
		if (!ClassName.empty() && ClassName != "None")
		{
			UObject* Created = FObjectFactory::Get().Create(ClassName, nullptr);
			Distribution = Cast<UDistributionVector>(Created);
		}
	}

	if (Distribution)
	{
		Distribution->Serialize(Ar);
	}

	return true;
}
