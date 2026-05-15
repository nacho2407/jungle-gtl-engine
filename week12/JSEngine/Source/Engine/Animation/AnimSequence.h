#pragma once

#include "Core/CoreMinimal.h"
#include "Object/Object.h"

using FVector3f = FVector;
using FQuat4f = FQuat;

struct FFrameRate
{
    int32 Numerator = 30;
    int32 Denominator = 1;

    float AsDecimal() const
    {
        return Denominator != 0 ? static_cast<float>(Numerator) / static_cast<float>(Denominator) : 0.0f;
    }
};

struct FRawAnimSequenceTrack
{
    TArray<FVector3f> PosKeys;
    TArray<FQuat4f> RotKeys;
    TArray<FVector3f> ScaleKeys;
};

struct FBoneAnimationTrack
{
    FName Name;
    FRawAnimSequenceTrack InternalTrack;
};

struct FAnimationCurveData
{
};

class UAnimationAsset : public UObject
{
public:
    DECLARE_CLASS(UAnimationAsset, UObject)

    UAnimationAsset() = default;
    ~UAnimationAsset() override = default;
};

class UAnimDataModel : public UObject
{
public:
    DECLARE_CLASS(UAnimDataModel, UObject)

    UAnimDataModel() = default;
    ~UAnimDataModel() override = default;

    virtual const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const;
    TArray<FBoneAnimationTrack>& GetMutableBoneAnimationTracks();

    float GetPlayLength() const { return PlayLength; }
    void SetPlayLength(float InPlayLength) { PlayLength = InPlayLength; }

    const FFrameRate& GetFrameRate() const { return FrameRate; }
    void SetFrameRate(const FFrameRate& InFrameRate) { FrameRate = InFrameRate; }

    int32 GetNumberOfFrames() const { return NumberOfFrames; }
    void SetNumberOfFrames(int32 InNumberOfFrames) { NumberOfFrames = InNumberOfFrames; }

    int32 GetNumberOfKeys() const { return NumberOfKeys; }
    void SetNumberOfKeys(int32 InNumberOfKeys) { NumberOfKeys = InNumberOfKeys; }

    const FAnimationCurveData& GetCurveData() const { return CurveData; }
    FAnimationCurveData& GetMutableCurveData() { return CurveData; }

private:
    TArray<FBoneAnimationTrack> BoneAnimationTracks;
    float PlayLength = 0.0f;
    FFrameRate FrameRate;
    int32 NumberOfFrames = 0;
    int32 NumberOfKeys = 0;
    FAnimationCurveData CurveData;
};

class UAnimSequenceBase : public UAnimationAsset
{
public:
    DECLARE_CLASS(UAnimSequenceBase, UAnimationAsset)

    UAnimSequenceBase() = default;
    ~UAnimSequenceBase() override = default;

    UAnimDataModel* GetDataModel() const { return DataModel; }
    UAnimDataModel* GetDataMode() const { return GetDataModel(); }
    void SetDataModel(UAnimDataModel* InDataModel) { DataModel = InDataModel; }

private:
    UAnimDataModel* DataModel = nullptr;
};

class UAnimSequence : public UAnimSequenceBase
{
public:
    DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

    UAnimSequence() = default;
    ~UAnimSequence() override = default;
};
