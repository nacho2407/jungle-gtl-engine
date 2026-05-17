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

    // UAnimSequenceBase:
    // 균등 간격 키 가정 (key i 의 시간 = i * PlayLength / (NumKeys - 1)).
    // Looping 이면 wrap, 아니면 clamp. 키 0개 → ref pose 유지, 1개 → 상수.
    // 회전 Slerp, 위치/스케일 Lerp. 본 별 BoneTreeIndex 가 Output.Pose 의 인덱스.
    void GetBonePose(FPoseContext& Output, const FAnimExtractContext& Ctx) const override;

    const TArray<FBoneAnimationTrack>& GetBoneTracks() const;
    TArray<FBoneAnimationTrack>& GetMutableBoneTracks();

    // 에디터 편집용: 직렬화 소스(DataModel->Notifies)에 직접 접근.
    // 편집 후 RefreshRuntimeNotifies() 로 dispatch 캐시(base Notifies)를 동기화한다.
    TArray<FAnimNotifyEvent>& GetMutableModelNotifies();
    void RefreshRuntimeNotifies();

    int32 TimeToFrame(float TimeSeconds) const;
    float FrameToTime(int32 FrameIndex) const;
    int32 GetNumberOfFrames() const;

    bool GetAnimationPose(float TimeSeconds, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose, bool bLooping = false) const;
    bool GetAnimationPoseAtFrame(int32 FrameIndex, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose) const;

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

    const FBoneAnimationTrack* FindBoneTrackByIndex(int32 BoneIndex) const;
    const FRawAnimSequenceTrack* FindTrackByBoneIndex(int32 TrackIndex) const;

    // ─────────────────────────────────────────────────────────────
    // Mock factories (A 의 FBX 임포트 전 시각 검증용 — 임시 데이터).
    // 두 팩토리 모두 UAnimDataModel 을 새로 만들고 SetDataModel 로 묶어 반환.
    // 반환된 UAnimSequence/UAnimDataModel 은 UObjectManager 가 소유 (수명 명시 관리 필요).
    // ─────────────────────────────────────────────────────────────

    // 특정 본 1개를 Z 축 기준으로 +Amp → 0 → -Amp → 0 sway 시킴. 5 키 (loop-safe).
    static UAnimSequence* CreateMockSwaySequence(
        USkeletalMesh* InMesh,
        int32 BoneIdx,
        float DurationSeconds = 1.5f,
        float AmplitudeDeg    = 30.0f);

    // 모든 본에 sinusoidal 회전. 본 인덱스로 위상차를 둬 wave 처럼 보이게.
    // TemporaryBoneAnimator 의 multi-bone 버전 재현용.
    static UAnimSequence* CreateMockWaveSequence(
        USkeletalMesh* InMesh,
        float DurationSeconds = 2.0f,
        float AmplitudeDeg    = 15.0f);

private:
    UAnimDataModel* DataModel = nullptr;

    FString AssetPathFileName = "None";
    FString SkeletonPath = "None";
    FString SkeletonGuid;
};
