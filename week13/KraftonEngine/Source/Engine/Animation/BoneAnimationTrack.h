#pragma once

#include "Core/CoreTypes.h"
#include "RawAnimSequenceTrack.h"
#include "Serialization/Archive.h"

// FRawAnimSequenceTrack 한 개를 특정 본에 묶어주는 매핑.
// BoneName은 저장 포맷의 안정적인 식별자이고, BoneTreeIndex는 런타임 평가용 캐시다.
struct FBoneAnimationTrack
{
    FString               BoneName;
    int32                 BoneTreeIndex = -1;
    FRawAnimSequenceTrack InternalTrackData;

    friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
    {
        Ar << Track.BoneName;
        Ar << Track.BoneTreeIndex;
        Ar << Track.InternalTrackData;
        return Ar;
    }
};
