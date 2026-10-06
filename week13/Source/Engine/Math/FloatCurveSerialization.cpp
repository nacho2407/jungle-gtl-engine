#include "Math/FloatCurveSerialization.h"

#include "Serialization/Archive.h"

void SerializeFloatCurve(FArchive& Ar, FFloatCurve& Curve)
{
    Ar << Curve.DefaultValue;

    int32 PreExtrap = static_cast<int32>(Curve.PreExtrapMode);
    int32 PostExtrap = static_cast<int32>(Curve.PostExtrapMode);
    Ar << PreExtrap;
    Ar << PostExtrap;

    if (Ar.IsLoading())
    {
        Curve.PreExtrapMode = static_cast<ECurveExtrapMode>(PreExtrap);
        Curve.PostExtrapMode = static_cast<ECurveExtrapMode>(PostExtrap);
    }

    Ar << Curve.Keys;

    if (Ar.IsLoading())
    {
        Curve.SortKeys();
        Curve.AutoSetTangents();
    }
}
