#pragma once

#include "Asset/CurveFloatAsset.h"
#include "Math/Vector.h"

class FVectorCurve
{
public:
	FVector Evaluate(float Time) const;
	void SortKeys();

	float GetStartTime() const;
	float GetEndTime() const;

	FFloatCurve XCurve;
	FFloatCurve YCurve;
	FFloatCurve ZCurve;
};

UCLASS()
class UCurveVectorAsset : public UObject
{
public:
	GENERATED_BODY(UCurveVectorAsset, UObject)

	FVector Evaluate(float Time) const;

	FVectorCurve& GetMutableCurve();
	const FVectorCurve& GetCurve() const;

	void SetAssetPath(const FString& InPath);
	const FString& GetAssetPath() const;

	void Serialize(FArchive& Ar) override;

private:
	FVectorCurve VectorCurve;
	FString AssetPath;
};
