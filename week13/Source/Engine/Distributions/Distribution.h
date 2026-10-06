#pragma once
#include "Object/Object.h"
#include "Core/Types/EngineTypes.h"
#include "Source/Engine/Distributions/Distribution.generated.h"

UCLASS(abstract)
class UDistribution : public UObject
{
public:
	GENERATED_BODY()

	virtual void GetRange(FVector& OutMin, FVector& OutMax) const {}
};
