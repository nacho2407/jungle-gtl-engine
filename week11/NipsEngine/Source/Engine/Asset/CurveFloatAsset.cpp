#include "Asset/CurveFloatAsset.h"

#include "Math/Utils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>

DEFINE_CLASS(UCurveFloatAsset, UObject)
REGISTER_FACTORY(UCurveFloatAsset)

float FFloatCurve::Evaluate(float Time) const
{
    if (Keys.empty())
    {
        return 0.0f;
    }

    if (Time <= Keys.front().Time)
    {
        return Keys.front().Value;
    }

    if (Time >= Keys.back().Time)
    {
        return Keys.back().Value;
    }

    for (int32 KeyIndex = 0; KeyIndex + 1 < static_cast<int32>(Keys.size()); ++KeyIndex)
    {
        const FCurveKey& StartKey = Keys[KeyIndex];
        const FCurveKey& EndKey = Keys[KeyIndex + 1];
        if (Time < StartKey.Time || Time > EndKey.Time)
        {
            continue;
        }

        const float SegmentLength = EndKey.Time - StartKey.Time;
        if (MathUtil::IsNearlyZero(SegmentLength))
        {
            return StartKey.Value;
        }

        const float Alpha = MathUtil::Clamp((Time - StartKey.Time) / SegmentLength, 0.0f, 1.0f);
        if (StartKey.InterpMode == ECurveInterpMode::Constant)
        {
            return StartKey.Value;
        }
        if (StartKey.InterpMode == ECurveInterpMode::Linear)
        {
            return StartKey.Value + (EndKey.Value - StartKey.Value) * Alpha;
        }

        const float Alpha2 = Alpha * Alpha;
        const float Alpha3 = Alpha2 * Alpha;
        const float H00 = 2.0f * Alpha3 - 3.0f * Alpha2 + 1.0f;
        const float H10 = Alpha3 - 2.0f * Alpha2 + Alpha;
        const float H01 = -2.0f * Alpha3 + 3.0f * Alpha2;
        const float H11 = Alpha3 - Alpha2;
        const float LeaveTangent = ResolveTangent(KeyIndex, true);
        const float ArriveTangent = ResolveTangent(KeyIndex + 1, false);

        return
            H00 * StartKey.Value +
            H10 * SegmentLength * LeaveTangent +
            H01 * EndKey.Value +
            H11 * SegmentLength * ArriveTangent;
    }

    return Keys.back().Value;
}

void FFloatCurve::SortKeys()
{
    std::sort(
        Keys.begin(),
        Keys.end(),
        [](const FCurveKey& A, const FCurveKey& B)
        {
            return A.Time < B.Time;
        });
}

float FFloatCurve::GetStartTime() const
{
    return Keys.empty() ? 0.0f : Keys.front().Time;
}

float FFloatCurve::GetEndTime() const
{
    return Keys.empty() ? 0.0f : Keys.back().Time;
}

float FFloatCurve::ResolveTangent(int32 KeyIndex, bool bLeaveTangent) const
{
    if (KeyIndex < 0 || KeyIndex >= static_cast<int32>(Keys.size()))
    {
        return 0.0f;
    }

    const FCurveKey& Key = Keys[KeyIndex];
    if (Key.TangentMode != ECurveTangentMode::Auto)
    {
        return bLeaveTangent ? Key.LeaveTangent : Key.ArriveTangent;
    }

    const int32 PrevIndex = KeyIndex - 1;
    const int32 NextIndex = KeyIndex + 1;
    if (PrevIndex >= 0 && NextIndex < static_cast<int32>(Keys.size()))
    {
        const float TimeDelta = Keys[NextIndex].Time - Keys[PrevIndex].Time;
        if (!MathUtil::IsNearlyZero(TimeDelta))
        {
            return (Keys[NextIndex].Value - Keys[PrevIndex].Value) / TimeDelta;
        }
    }

    const int32 NeighborIndex = bLeaveTangent ? NextIndex : PrevIndex;
    if (NeighborIndex >= 0 && NeighborIndex < static_cast<int32>(Keys.size()))
    {
        const float TimeDelta = Keys[NeighborIndex].Time - Key.Time;
        if (!MathUtil::IsNearlyZero(TimeDelta))
        {
            return (Keys[NeighborIndex].Value - Key.Value) / TimeDelta;
        }
    }

    return 0.0f;
}

float UCurveFloatAsset::Evaluate(float Time) const
{
    return FloatCurve.Evaluate(Time);
}

FFloatCurve& UCurveFloatAsset::GetMutableCurve()
{
    return FloatCurve;
}

const FFloatCurve& UCurveFloatAsset::GetCurve() const
{
    return FloatCurve;
}

void UCurveFloatAsset::SetAssetPath(const FString& InPath)
{
    AssetPath = InPath;
}

const FString& UCurveFloatAsset::GetAssetPath() const
{
    return AssetPath;
}

void UCurveFloatAsset::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
    Ar << "AssetPath" << AssetPath;

    TArray<float> Times;
    TArray<float> Values;
    TArray<int32> InterpModes;
    TArray<int32> TangentModes;
    TArray<float> ArriveTangents;
    TArray<float> LeaveTangents;

    if (Ar.IsSaving())
    {
        Times.reserve(FloatCurve.Keys.size());
        Values.reserve(FloatCurve.Keys.size());
        InterpModes.reserve(FloatCurve.Keys.size());
        TangentModes.reserve(FloatCurve.Keys.size());
        ArriveTangents.reserve(FloatCurve.Keys.size());
        LeaveTangents.reserve(FloatCurve.Keys.size());

        for (const FCurveKey& Key : FloatCurve.Keys)
        {
            Times.push_back(Key.Time);
            Values.push_back(Key.Value);
            InterpModes.push_back(static_cast<int32>(Key.InterpMode));
            TangentModes.push_back(static_cast<int32>(Key.TangentMode));
            ArriveTangents.push_back(Key.ArriveTangent);
            LeaveTangents.push_back(Key.LeaveTangent);
        }
    }

    Ar << "Times" << Times;
    Ar << "Values" << Values;
    Ar << "InterpModes" << InterpModes;
    Ar << "TangentModes" << TangentModes;
    Ar << "ArriveTangents" << ArriveTangents;
    Ar << "LeaveTangents" << LeaveTangents;

    if (Ar.IsLoading())
    {
        const size_t KeyCount = Times.size();
        FloatCurve.Keys.clear();
        FloatCurve.Keys.reserve(KeyCount);
        for (size_t Index = 0; Index < KeyCount; ++Index)
        {
            FCurveKey Key;
            Key.Time = Times[Index];
            Key.Value = Index < Values.size() ? Values[Index] : 0.0f;
            Key.InterpMode = Index < InterpModes.size()
                ? static_cast<ECurveInterpMode>(InterpModes[Index])
                : ECurveInterpMode::Cubic;
            Key.TangentMode = Index < TangentModes.size()
                ? static_cast<ECurveTangentMode>(TangentModes[Index])
                : ECurveTangentMode::Auto;
            Key.ArriveTangent = Index < ArriveTangents.size() ? ArriveTangents[Index] : 0.0f;
            Key.LeaveTangent = Index < LeaveTangents.size() ? LeaveTangents[Index] : 0.0f;
            FloatCurve.Keys.push_back(Key);
        }
        FloatCurve.SortKeys();
    }
}
