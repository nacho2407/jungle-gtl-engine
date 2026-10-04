#pragma once

#include "Object/Object.h"
#include "Asset/SkeletalMeshTypes.h"

class USkeleton : public UObject
{
public:
    DECLARE_CLASS(USkeleton, UObject)

    const FReferenceSkeleton& GetReferenceSkeleton() const { return ReferenceSkeleton; }
    FReferenceSkeleton& GetReferenceSkeleton() { return ReferenceSkeleton; }

    void SetReferenceSkeleton(const FReferenceSkeleton& InReferenceSkeleton);

private:
    FReferenceSkeleton ReferenceSkeleton;
};
