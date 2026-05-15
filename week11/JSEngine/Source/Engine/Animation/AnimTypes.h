#pragma once
#include "Core/CoreMinimal.h"
#include "Core/Guid.h"
#include "Core/PropertyTypes.h"
#include <Object/FName.h>
#include "Object/Object.h"
class USkeletalMesh;
struct FRawAnimSequenceTrack
{
    TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
    TArray<FVector> ScaleKeys;

    TArray<float> PosKeyTimes;
    TArray<float> RotKeyTimes;
    TArray<float> ScaleKeyTimes;
};
struct FAnimationTrack
{
    FName BoneName;
    int32 BoneIndex = -1;
    FRawAnimSequenceTrack InternalTrackData;
};

class UAnimDataModel : public UObject
{
public:
    DECLARE_CLASS(UAnimDataModel, UObject)
	float SequnceLength = 0.0f;
    float FrameRate = 30.0f;
    int32 NumberOfFrames = 0;
    TArray<FAnimationTrack> BoneAnimationTracks;

	
    const FAnimationTrack* FindTrackByBoneIndex(int32 BoneIndex) const;
    const FAnimationTrack* FindTrackByBoneName(const FName& BoneName) const;
};

struct FAnimSequenceBinaryHeader
{
    uint32 MagicNumber = 0x4D494E41; 
    uint32 Version = 1;

    uint64 SourceFileWriteTime = 0;

    float SequenceLength = 0.0f;
    float FrameRate = 30.0f;
    uint32 FrameCount = 0;
    uint32 TrackCount = 0;
};
class UAnimSequence : public UObject
{
public:
    DECLARE_CLASS(UAnimSequence, UObject)

	 ~UAnimSequence() override;

    FString AssetPath;
    FString SourceFbxPath;
    FString TargetSkeletonPath; 
    UAnimDataModel* DataModel = nullptr;

    bool GetBonePose(float Time, const USkeletalMesh* Mesh, TArray<FMatrix>& OutLocalPose) const;

private:
    static FVector EvalVectorKeys(const TArray<FVector>& Keys, const TArray<float>& Times,float Time, const FVector& DefaultValue);
    static FQuat EvalQautKeys(const TArray<FQuat>& Keys, const TArray<float>& Times, float Time, const FQuat& DefaultValue);
};
