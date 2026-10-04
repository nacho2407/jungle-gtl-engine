#include "Skeleton.h"

DEFINE_CLASS(USkeleton, UObject)

void USkeleton::SetReferenceSkeleton(const FReferenceSkeleton& InReferenceSkeleton)
{
    ReferenceSkeleton = InReferenceSkeleton;
    ReferenceSkeleton.RebuildNameToIndex();
}
