#pragma once
#include "Object/Object.h"
#include "Math/FloatCurve.h"

class FArchive;

class UFloatCurveAsset : public UObject
{
public:
	DECLARE_CLASS(UFloatCurveAsset, UObject)

	UFloatCurveAsset() = default;
	~UFloatCurveAsset() override;

	FFloatCurve& GetCurve() { return Curve; }
	const FFloatCurve& GetCurve() const { return Curve; }

	void SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const { return SourcePath; }

	void Serialize(FArchive& Ar) override;

private:
	FFloatCurve Curve;
	FString SourcePath;
};
