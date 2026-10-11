#include "FloatCurveAsset.h"
#include "Platform/Paths.h"
#include "Object/Reflection/ObjectFactory.h"

#include "Serialization/Archive.h"

#include <fstream>
#include <sstream>

UFloatCurveAsset::~UFloatCurveAsset()
{
}

void UFloatCurveAsset::Serialize(FArchive& Ar)
{
	Ar << Curve;
}
