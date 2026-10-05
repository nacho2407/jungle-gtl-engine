#pragma once

#include "Asset/CurveFloatAsset.h"
#include "Math/Color.h"

class FColorCurve
{
public:
	FColor Evaluate(float Time) const;
	void SortKeys();

	float GetStartTime() const;
	float GetEndTime() const;

	FFloatCurve RCurve;
	FFloatCurve GCurve;
	FFloatCurve BCurve;
	FFloatCurve ACurve;
};

UCLASS()
class UCurveColorAsset : public UObject
{
public:
	GENERATED_BODY(UCurveColorAsset, UObject)

	FColor Evaluate(float Time) const;

	FColorCurve& GetMutableCurve();
	const FColorCurve& GetCurve() const;

	void SetAssetPath(const FString& InPath);
	const FString& GetAssetPath() const;

	void Serialize(FArchive& Ar) override;

private:
	FColorCurve ColorCurve;
	FString AssetPath;
};
