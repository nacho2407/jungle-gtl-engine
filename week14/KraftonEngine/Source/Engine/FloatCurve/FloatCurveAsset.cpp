#include "FloatCurveAsset.h"
#include "Platform/Paths.h"
#include "Object/ObjectFactory.h"

#include "Serialization/Archive.h"

#include <fstream>
#include <sstream>

IMPLEMENT_CLASS(UFloatCurveAsset, UObject)

UFloatCurveAsset::~UFloatCurveAsset()
{
}

static FArchive& operator<<(FArchive& Ar, FCurveKey& Key)
{
	Ar << Key.Time;
	Ar << Key.Value;

	int32 InterpMode = static_cast<int32>(Key.InterpMode);
	Ar << InterpMode;
	if (Ar.IsLoading())
	{
		Key.InterpMode = static_cast<ECurveInterpMode>(InterpMode);
	}

	int32 TangentMode = static_cast<int32>(Key.TangentMode);
	Ar << TangentMode;
	if (Ar.IsLoading())
	{
		Key.TangentMode = static_cast<ECurveTangentMode>(TangentMode);
	}

	Ar << Key.ArriveTangent;
	Ar << Key.LeaveTangent;

	return Ar;
}

void UFloatCurveAsset::Serialize(FArchive& Ar)
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
	}
}
