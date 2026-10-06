#include "DistributionVector.h"
#include "Math/RandomStream.h"
#include "Serialization/Archive.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/GarbageCollection.h"
#include "Math/FloatCurveSerialization.h"
#include <cstdlib>
#include <algorithm>

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


static void InitializeVectorCurveChannel(FFloatCurve& Curve, float InitialValue)
{
	Curve.Reset();
	Curve.DefaultValue = InitialValue;
	Curve.AddKey(0.0f, InitialValue);
	Curve.AddKey(1.0f, InitialValue);
	Curve.SortKeys();
	Curve.AutoSetTangents();
}

static void GetVectorCurveChannelRange(const FFloatCurve& Curve, float& OutMin, float& OutMax)
{
	if (Curve.Keys.empty())
	{
		OutMin = OutMax = Curve.DefaultValue;
		return;
	}

	OutMin = Curve.Keys.front().Value;
	OutMax = Curve.Keys.front().Value;
	for (const FCurveKey& Key : Curve.Keys)
	{
		OutMin = (std::min)(OutMin, Key.Value);
		OutMax = (std::max)(OutMax, Key.Value);
	}
}

UDistributionVectorCurve::UDistributionVectorCurve()
{
	InitializeVectorCurveChannel(X, 0.0f);
	InitializeVectorCurveChannel(Y, 0.0f);
	InitializeVectorCurveChannel(Z, 0.0f);
}

FVector UDistributionVectorCurve::GetValue(float Time, UObject* Data, FRandomStream* InRandomStream) const
{
	return FVector(X.Evaluate(Time), Y.Evaluate(Time), Z.Evaluate(Time));
}

void UDistributionVectorCurve::GetRange(FVector& OutMin, FVector& OutMax) const
{
	GetVectorCurveChannelRange(X, OutMin.X, OutMax.X);
	GetVectorCurveChannelRange(Y, OutMin.Y, OutMax.Y);
	GetVectorCurveChannelRange(Z, OutMin.Z, OutMax.Z);
}

void UDistributionVectorCurve::Serialize(FArchive& Ar)
{
	UDistributionVector::Serialize(Ar);
	SerializeFloatCurve(Ar, X);
	SerializeFloatCurve(Ar, Y);
	SerializeFloatCurve(Ar, Z);
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
