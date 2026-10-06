#include "DistributionFloat.h"
#include "Math/RandomStream.h"
#include "Serialization/Archive.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/GarbageCollection.h"
#include "Math/FloatCurveSerialization.h"
#include <cstdlib>
#include <algorithm>

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


static void GetFloatCurveRange(const FFloatCurve& Curve, float& OutMin, float& OutMax)
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

static void InitializeFloatCurve(FFloatCurve& Curve, float InitialValue)
{
	Curve.Reset();
	Curve.DefaultValue = InitialValue;
	Curve.AddKey(0.0f, InitialValue);
	Curve.AddKey(1.0f, InitialValue);
	Curve.SortKeys();
	Curve.AutoSetTangents();
}

UDistributionFloatCurve::UDistributionFloatCurve()
{
	InitializeFloatCurve(Curve, 0.0f);
}

float UDistributionFloatCurve::GetValue(float Time, UObject* Data, FRandomStream* InRandomStream) const
{
	return Curve.Evaluate(Time);
}

void UDistributionFloatCurve::GetRange(float& OutMin, float& OutMax) const
{
	GetFloatCurveRange(Curve, OutMin, OutMax);
}

void UDistributionFloatCurve::Serialize(FArchive& Ar)
{
	UDistributionFloat::Serialize(Ar);
	SerializeFloatCurve(Ar, Curve);
}

void FRawDistributionFloat::AddReferencedObjects(FReferenceCollector& Collector) const
{
	Collector.AddReferencedObject(Distribution, "FRawDistributionFloat.Distribution");
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
			UObject* Created = FObjectFactory::Get().Create(ClassName, nullptr); // Outer will be set by caller or during create
			Distribution = Cast<UDistributionFloat>(Created);
		}
	}

	if (Distribution)
	{
		Distribution->Serialize(Ar);
	}

	return true;
}
