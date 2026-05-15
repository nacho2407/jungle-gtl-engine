#pragma once

#include "AnimSequenceBase.h"
#include "BoneAnimationTrack.h"

struct FTransform;
class USkeletalMesh;
class USkeleton;
class UAnimDataModel;

// 본별 키프레임을 가진 표준 시퀀스.
// 실제 애니메이션 데이터는 UAnimDataModel 하나만 소유하고, 이 클래스는 평가/호환성/에셋 메타데이터를 담당한다.
class UAnimSequence : public UAnimSequenceBase
{
public:
    DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

    UAnimSequence() = default;
    ~UAnimSequence() override = default;

    void Serialize(FArchive& Ar) override;

    void SetDataModel(UAnimDataModel* InModel);

    UAnimDataModel* GetDataModel() const
    {
        return DataModel;
    }

    void GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const override;

    const TArray<FBoneAnimationTrack>& GetBoneTracks() const;
    TArray<FBoneAnimationTrack>& GetMutableBoneTracks();

    int32 TimeToFrame(float TimeSeconds) const;
    float FrameToTime(int32 FrameIndex) const;
    int32 GetNumberOfFrames() const;

    bool GetAnimationPose(float TimeSeconds, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose) const;

    const FString& GetSkeletonPath() const
    {
        return SkeletonPath;
    }

    void SetSkeletonPath(const FString& InSkeletonPath)
    {
        SkeletonPath = InSkeletonPath;
    }

    const FString& GetSkeletonGuid() const
    {
        return SkeletonGuid;
    }

    void SetSkeletonGuid(const FString& InSkeletonGuid)
    {
        SkeletonGuid = InSkeletonGuid;
    }

    bool IsCompatibleWith(const USkeleton* InSkeleton) const;
    bool IsCompatibleWith(const USkeletalMesh* InSkeletalMesh) const;

    const FString& GetAssetPathFileName() const
    {
        return AssetPathFileName;
    }

    void SetAssetPathFileName(const FString& InPath)
    {
        AssetPathFileName = InPath;
    }

private:
    UAnimDataModel* DataModel = nullptr;

    FString AssetPathFileName = "None";
    FString SkeletonPath = "None";
    FString SkeletonGuid;
};
