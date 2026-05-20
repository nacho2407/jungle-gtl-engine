#pragma once

#include "AnimSequenceBase.h"
#include "BoneAnimationTrack.h"
#include "SkeletonTypes.h"
#include "Math/Transform.h"

class USkeletalMesh;
class USkeleton;
class UAnimDataModel;

// 본별 키프레임을 가진 표준 시퀀스.
// 실제 애니메이션 데이터는 UAnimDataModel 하나만 소유하고, 이 클래스는 평가/호환성/에셋 메타데이터를 담당한다.

#include "Source/Engine/Animation/AnimSequence.generated.h"

UCLASS()
class UAnimSequence : public UAnimSequenceBase
{
public:
	GENERATED_BODY()
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

    const TArray<FMorphTargetCurve>& GetMorphTargetCurves() const;
    TArray<FMorphTargetCurve>&       GetMutableMorphTargetCurves();

    void EvaluateMorphTargetCurves(
        float          TimeSeconds,
        bool           bLooping,
        USkeletalMesh* InSkeletalMesh,
        TArray<float>& OutWeights
        ) const;

    // 에디터 편집용: 직렬화 소스(DataModel->Notifies)에 직접 접근.
    // 편집 후 RefreshRuntimeNotifies() 로 dispatch 캐시(base Notifies)를 동기화한다.
    TArray<FAnimNotifyEvent>& GetMutableModelNotifies();
    void RefreshRuntimeNotifies();

    int32 TimeToFrame(float TimeSeconds) const;
    float FrameToTime(int32 FrameIndex) const;
    int32 GetNumberOfFrames() const;

    bool GetAnimationPose(float TimeSeconds, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose, bool bLooping = false) const;
    bool GetAnimationPoseAtFrame(int32 FrameIndex, USkeletalMesh* InSkeletalMesh, TArray<FTransform>& OutLocalPose) const;

    const FSkeletonBinding& GetSkeletonBinding() const
    {
        return TargetSkeleton;
    }

    void SetSkeletonBinding(const FSkeletonBinding& InBinding)
    {
        TargetSkeleton = InBinding;
        if (TargetSkeleton.SkeletonPath.empty())
        {
            TargetSkeleton.SkeletonPath = "None";
        }
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
    // Force Root Lock 옵션 (UE 의 bForceRootLock 와 동등, per-asset, .uasset 에 저장).
    //   bForceRootLock = true 면 GetBonePose 시 RootMotionBoneName 본의
    //   local translation 의 horizontal (X/Y) 성분을 bind 값으로 고정 (vertical Z 는 anim 유지).
    //   → walk/run 류 anim 의 forward 이동을 pose 평가 단계에서 잠가 in-place 로 재생.
    // 참고: UE 에서 "Root Motion" 은 anim 의 root 데이터를 character actor transform 에 반영하는
    //       별개 옵션 (bEnableRootMotion). 본 기능은 그것과 다르고, root 본 motion 을 잠그는 것.
    // RootMotionBoneName 은 import 시 자동 감지 (translation 변화가 큰 상위 본 = 보통 hip/Bip001).
    // 사용자는 AnimViewer 에서 수동 override 가능.
    // ─────────────────────────────────────────────────────────────
    bool GetForceRootLock() const { return bForceRootLock; }
    void SetForceRootLock(bool b) { bForceRootLock = b; }

    const FString& GetRootMotionBoneName() const { return RootMotionBoneName; }
    void SetRootMotionBoneName(const FString& Name) { RootMotionBoneName = Name; }

    // ─────────────────────────────────────────────────────────────
    // Root Motion (UE 의 bEnableRootMotion 과 동등).
    //   true 면 GetBonePose 에서 RootMotionBoneName 본의 translation 을 bind 로 고정
    //   (= Force Root Lock 의 strict 버전) 하고, 대신 ExtractRootMotion 으로 delta 를
    //   추출해 AnimInstance 가 owning actor 의 transform 에 반영한다.
    //   → 캐릭터가 anim 의 motion 으로 실제로 world 에서 움직임.
    // Force Root Lock 과 상호 배제 (둘 다 root translation 을 다루므로 동시 활성 안 됨).
    // ─────────────────────────────────────────────────────────────
    bool GetEnableRootMotion() const { return bEnableRootMotion; }
    void SetEnableRootMotion(bool b) { bEnableRootMotion = b; if (b) bForceRootLock = false; }

    // [PrevTime, CurTime) 구간에서 root motion 본의 local translation/rotation delta 를 추출.
    //   bLoop 면 시간이 끝에서 0 으로 wrap 되는 경계도 정확하게 누적 (두 구간 합산).
    //   RootMotionBoneName 비어있거나 본 track 못 찾으면 Identity 반환.
    //   delta 는 anim 의 본 local 좌표계 → 호출자가 actor world 로 변환해야 함.
    FTransform ExtractRootMotion(float PrevTime, float CurTime, bool bLoop) const;

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
    FSkeletonBinding TargetSkeleton;

    bool    bForceRootLock    = false;
    bool    bEnableRootMotion = false;
    FString RootMotionBoneName;
};
