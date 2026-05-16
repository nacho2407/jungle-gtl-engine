#include "Animation/Skeleton.h"

DEFINE_CLASS(USkeleton, UObject)

void USkeleton::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);

    Ar << AssetPathFileName;
    Ar << SkeletonGuid;
    Ar << ReferenceSkeleton;
}
