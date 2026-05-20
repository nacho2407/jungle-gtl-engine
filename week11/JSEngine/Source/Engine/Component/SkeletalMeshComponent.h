#pragma once

#include "SkeletalMeshComponent.generated.h"

#include "Animation/AnimationTypes.h"
#include "Core/Containers/Set.h"
#include "Core/Delegates/Delegate.h"
#include "Component/SkinnedMeshComponent.h"

class UAnimInstance;
class UAnimSingleNodeInstance;
class UAnimStateMachineInstance;
class UAnimationAsset;
class USkeletalMeshComponent;
struct FTransform;

DECLARE_DELEGATE(FOnAnimNotify, USkeletalMeshComponent*, const FAnimNotifyDispatchEvent&)

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
    GENERATED_BODY_USkeletalMeshComponent()
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override;

    FOnAnimNotify OnAnimNotify;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;
    void TickComponent(float DeltaTime) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

    void SetAnimationMode(EAnimationMode InMode);
    EAnimationMode GetAnimationMode() const;
    void SetAnimInstanceClass(const FString& InClassName);
    UAnimInstance* GetAnimInstance() const;
    void RecreateAnimInstance();
    bool ApplyAnimationLocalPose(const TArray<FTransform>& LocalPose);

	/**
	 * @brief 명시적으로 animation pose를 업데이트하는 함수
	 *
	 * @note SetPosition은 다음 프레임 TickComponent에서 실제 pose 적용이 일어남
	 *        
	 * @example animation sequence viewer에서 timeline scrubber를 사용할 때 슬라이더를 움직인 즉시 화면을 갱신
	 */
    bool RefreshAnimationPose();

    void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);
    void SetAnimation(UAnimationAsset* NewAnimToPlay);
    UAnimationAsset* GetAnimation() const;
    void Play();
    void Pause();
    void Stop();
    void SetPosition(float TimeSeconds, bool bFireNotifies = false);
    float GetPosition() const;
    float GetPreviousTime() const;
    void SetPlayRate(float InPlayRate);
    float GetPlayRate() const;
    void SetReversePlay(bool bInReversePlay);
    bool IsReversePlay() const;
    void SetLooping(bool bInLooping);
    bool IsLooping() const;
    bool IsPlaying() const;
    bool IsPaused() const;
    float GetPlayLength() const;
    void SetRootMotionMode(ERootMotionMode InMode);
    ERootMotionMode GetRootMotionMode() const;
    FRootMotionDelta GetLastExtractedRootMotion() const;
    void SetRootMotionBoneIndex(int32 InBoneIndex);
    int32 GetRootMotionBoneIndex() const;
    void SetRootMotionBoneName(const FName& InBoneName);
    FName GetRootMotionBoneName() const;

    bool SetAnimSequence(const FString& SourceFbxPath, const FString& AnimStackName = FString());
    bool SetAnimSequenceAsset(const FString& AssetPath);
    const FString& GetAnimSequenceAssetPath() const { return AnimSequenceAssetPath; }
    const FString& GetAnimSequenceSourceFbxPath() const { return AnimSequenceSourceFbxPath; }
    const FString& GetAnimSequenceStackName() const { return AnimSequenceStackName; }
    bool SetAnimStateMachine(const FString& Path);
    UAnimStateMachineInstance* GetStateMachineInstance() const;
    void SetAnimVariableFloat(const FName& Name, float Value);
    float GetAnimVariableFloat(const FName& Name, float DefaultValue = 0.0f) const;
    void SetAnimVariableBool(const FName& Name, bool Value);
    bool GetAnimVariableBool(const FName& Name, bool DefaultValue = false) const;
    FName GetCurrentAnimStateName() const;
    FName GetPreviousAnimStateName() const;
    FName GetTargetAnimStateName() const;
    float GetAnimTransitionAlpha() const;
    bool IsAnimTransitioning() const;
    void SetAnimationTime(float Time);
    void TickAnimation(float DeltaTime);
    void PlayAnim(bool bLoop);
    void StopAnim();
    float GetAnimationTime() const { return GetPosition(); }
    bool IsAnimPlaying() const { return IsPlaying(); }
    void HandleAnimNotify(const FAnimNotifyDispatchEvent& NotifyEvent);
    bool HasLastAnimNotifyEvent() const;
    const FAnimNotifyDispatchEvent& GetLastAnimNotifyEvent() const;
    void ClearLastAnimNotifyEvent();

    void ResetToBindPose();

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

private:
    void DestroyAnimInstance();
    UAnimSingleNodeInstance* GetSingleNodeInstance() const;
    UAnimSingleNodeInstance* EnsureSingleNodeInstance();
    void WarnPoseBoneCountMismatchOnce(int32 MeshBoneCount, int32 PoseBoneCount);

private:
    EAnimationMode AnimationMode = EAnimationMode::None;
    FString AnimInstanceClassName;
    FString AnimSequenceAssetPath;
    FString AnimSequenceSourceFbxPath;
    FString AnimSequenceStackName;
    UAnimInstance* AnimInstance = nullptr;

	// for stat / debug
    FAnimNotifyDispatchEvent LastAnimNotifyEvent;
    bool bHasLastAnimNotifyEvent = false;
    TSet<FString> PoseBoneCountMismatchWarningKeys;
};
