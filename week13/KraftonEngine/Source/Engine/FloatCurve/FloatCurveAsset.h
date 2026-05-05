#pragma once
#include "Object/Object.h"
#include "Math/FloatCurve.h"

class UFloatCurveAsset : public UObject
{
public:
	DECLARE_CLASS(UFloatCurveAsset, UObject)

	UFloatCurveAsset() = default;
	~UFloatCurveAsset() override;

	bool LoadFromFile(const FString& Path);
	bool SaveToFile(const FString& Path) const;

	FFloatCurve& GetCurve() { return Curve; }
	const FFloatCurve& GetCurve() const { return Curve; }

	void SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const { return SourcePath; }

private:
	FFloatCurve Curve;
	FString SourcePath;
};
