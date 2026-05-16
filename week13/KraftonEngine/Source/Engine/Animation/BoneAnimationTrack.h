#pragma once

#include "Core/CoreTypes.h"
#include "RawAnimSequenceTrack.h"
#include "Serialization/Archive.h"

// FRawAnimSequenceTrack 한 개를 특정 본 인덱스에 묶어주는 매핑.
// AnimSequence 는 TArray<FBoneAnimationTrack> 를 가지고, BoneTreeIndex 는
// SkeletalMesh::Bones 배열 인덱스와 동일하게 유지한다.
struct FBoneAnimationTrack
{
    int32                 BoneTreeIndex = -1;
    FRawAnimSequenceTrack InternalTrackData;

    friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
    {
        Ar << Track.BoneTreeIndex;
        Ar << Track.InternalTrackData;
        return Ar;
    }
};
