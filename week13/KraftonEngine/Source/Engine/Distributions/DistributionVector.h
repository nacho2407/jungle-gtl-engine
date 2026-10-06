#pragma once
#include "Distributions.h"
#include "Distribution.h"
#include "Object/Reflection/ObjectMacros.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Core/Types/EngineTypes.h"
#include "Math/FloatCurve.h"

class FReferenceCollector;

#include "Source/Engine/Distributions/DistributionVector.generated.h"

UCLASS(abstract)
class UDistributionVector : public UDistribution
{
public:
	GENERATED_BODY()

	virtual FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, struct FRandomStream* InRandomStream = nullptr) const { return FVector::ZeroVector; }

	virtual void GetRange(FVector& OutMin, FVector& OutMax) const override { OutMin = OutMax = FVector::ZeroVector; }

	virtual void Serialize(FArchive& Ar) override;
};

UCLASS()
class UDistributionVectorConstant : public UDistributionVector
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DistributionVectorConstant")
	FVector Constant;

	virtual FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, struct FRandomStream* InRandomStream = nullptr) const override { return Constant; }
	virtual void GetRange(FVector& OutMin, FVector& OutMax) const override { OutMin = OutMax = Constant; }

	virtual void Serialize(FArchive& Ar) override;

	UDistributionVectorConstant() : Constant(FVector::ZeroVector) {}
};

UCLASS()
class UDistributionVectorUniform : public UDistributionVector
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "DistributionVectorUniform")
	FVector Min;

	UPROPERTY(EditAnywhere, Category = "DistributionVectorUniform")
	FVector Max;

	UPROPERTY(EditAnywhere, Category = "DistributionVectorUniform")
	bool bLockAxes = false;

	virtual FVector GetValue(float Time = 0.f, UObject* Data = NULL, struct FRandomStream* InRandomStream = NULL) const override;
	virtual void GetRange(FVector& OutMin, FVector& OutMax) const override { OutMin = Min; OutMax = Max; }

	virtual void Serialize(FArchive& Ar) override;

	UDistributionVectorUniform() : Min(FVector::ZeroVector), Max(FVector::ZeroVector) {}
};


UCLASS()
class UDistributionVectorCurve : public UDistributionVector
{
public:
	GENERATED_BODY()

	FFloatCurve X;
	FFloatCurve Y;
	FFloatCurve Z;

	UDistributionVectorCurve();

	virtual FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, struct FRandomStream* InRandomStream = nullptr) const override;
	virtual void GetRange(FVector& OutMin, FVector& OutMax) const override;

	virtual void Serialize(FArchive& Ar) override;
};

USTRUCT()
struct FRawDistributionVector : public FRawDistribution
{
	GENERATED_BODY()
private:
	UPROPERTY()
	float MinValue;

	UPROPERTY()
	float MaxValue;

	UPROPERTY()
	FVector MinValueVec;

	UPROPERTY()
	FVector MaxValueVec;

public:
	// Instanced runtime distribution object. Serialized by FRawDistributionVector::Serialize as class + payload.
	UPROPERTY(EditAnywhere, Instanced, Category = "RawDistributionVector")
	TObjectPtr<UDistributionVector> Distribution;

	/** Whether the distribution data has been cooked or the object itself is available */
	bool IsCreated() const { return Distribution != nullptr; }

	/**
	 * 
	 * \param Time
	 * \param Data: Distribution Data
	 * \param InRandomStream
	 * \return 
	 */
	FVector GetValue(float Time = 0.0f, UObject* Data = nullptr, struct FRandomStream* InRandomStream = nullptr) const;

	void AddReferencedObjects(FReferenceCollector& Collector) const;

	bool Serialize(FArchive& Ar);

	FRawDistributionVector()
		: MinValue(0)
		, MaxValue(0)
		, MinValueVec(FVector::ZeroVector)
		, MaxValueVec(FVector::ZeroVector)
		, Distribution(NULL)
	{}
};
