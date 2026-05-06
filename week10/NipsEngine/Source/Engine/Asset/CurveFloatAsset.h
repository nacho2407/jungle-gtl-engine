#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/String.h"
#include "Object/Object.h"

enum class ECurveInterpMode : uint8
{
    Constant = 0,
    Linear,
    Cubic,
};

enum class ECurveTangentMode : uint8
{
    Auto = 0,
    User,
    Break,
};

struct FCurveKey
{
    float Time = 0.0f;
    float Value = 0.0f;

    ECurveInterpMode InterpMode = ECurveInterpMode::Cubic;
    ECurveTangentMode TangentMode = ECurveTangentMode::Auto;

    float ArriveTangent = 0.0f;
    float LeaveTangent = 0.0f;
};

class FFloatCurve
{
public:
    TArray<FCurveKey> Keys;

    float Evaluate(float Time) const;
    void SortKeys();

    float GetStartTime() const;
    float GetEndTime() const;

private:
    float ResolveTangent(int32 KeyIndex, bool bLeaveTangent) const;
};

class UCurveFloatAsset : public UObject
{
public:
    DECLARE_CLASS(UCurveFloatAsset, UObject)

    float Evaluate(float Time) const;

    FFloatCurve& GetMutableCurve();
    const FFloatCurve& GetCurve() const;

    void SetAssetPath(const FString& InPath);
    const FString& GetAssetPath() const;

    void Serialize(FArchive& Ar) override;

private:
    FFloatCurve FloatCurve;
    FString AssetPath;
};
