#pragma once
#include "Distributions.h"
#include "Distribution.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Core/Types/EngineTypes.h"
#include "Math/RandomStream.h"

class FReferenceCollector;

#include "Source/Engine/Distributions/DistributionFloat.generated.h"

UCLASS(abstract)
class UDistributionFloat : public UDistribution
{
public:
	GENERATED_BODY()

	virtual float GetValue(float Time = 0.f, UObject* Data = NULL, struct FRandomStream* InRandomStream = NULL) const { return 0.f; }

	virtual void GetRange(float& OutMin, float& OutMax) const { OutMin = OutMax = 0.f; }

	virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class UDistributionFloatConstant : public UDistributionFloat
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DistributionFloatConstant")
	float Constant;

	virtual float GetValue(float Time = 0.f, UObject* Data = NULL, struct FRandomStream* InRandomStream = NULL) const override { return Constant; }
	virtual void GetRange(float& OutMin, float& OutMax) const override { OutMin = OutMax = Constant; }

	virtual void Serialize(FArchive& Ar) override;

	UDistributionFloatConstant() : Constant(0.f) {}
};

UCLASS()
class UDistributionFloatUniform : public UDistributionFloat
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DistributionFloatUniform")
	float Min;

	UPROPERTY(EditAnywhere, Category = "DistributionFloatUniform")
	float Max;

	virtual float GetValue(float Time = 0.f, UObject* Data = NULL, struct FRandomStream* InRandomStream = NULL) const override;
	virtual void GetRange(float& OutMin, float& OutMax) const override { OutMin = Min; OutMax = Max; }

	virtual void Serialize(FArchive& Ar) override;

	UDistributionFloatUniform() : Min(0.f), Max(0.f) {}
};

USTRUCT()
struct FRawDistributionFloat : public FRawDistribution
{
	GENERATED_BODY()
private:
	UPROPERTY()
	float MinValue;

	UPROPERTY()
	float MaxValue;

public:
	UPROPERTY(EditAnywhere, Category = "RawDistributionFloat")
	UDistributionFloat* Distribution;

	/** Whether the distribution data has been cooked or the object itself is available */
	bool IsCreated() const { return Distribution != nullptr; }

	float GetValue(float Time = 0.0f, UObject* Data = nullptr, struct FRandomStream* InRandomStream = nullptr) const;

	void AddReferencedObjects(FReferenceCollector& Collector) const;

	bool Serialize(FArchive& Ar);

	FRawDistributionFloat()
		: MinValue(0),
		MaxValue(0),
		Distribution(NULL)
	{}
};
