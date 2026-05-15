#pragma once

#include "Component/SkinnedMeshComponent.h"
#include "Core/Delegates/Delegate.h"

struct FPoseContext;
class UAnimInstance;
class UAnimSequenceBase;
class UAnimationAsset;
struct FAnimNotifyEvent;

enum class EAnimationMode
{
    AnimationBlueprint,
    AnimationSingleNode,
    AnimationCustomMode
};

/**
 * @brief Unreal Engine 스타일에서는 skinned mesh가 skeleton을 이용하는 mesh를 표현하고,
 *        skeletal mesh는 실제로 actor에 붙어서 애니메이션을 붙일 수 있는 component로 사용되고 있으므로
 *        USkeletalMeshComponent 또한 해당 방식대로 우선은 얇게 유지.
 *        핵심 로직들은 대부분 USkinnedMeshComponent로 옮겼습니다.
 */
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_DELEGATE(FOnAnimNotify, USkeletalMeshComponent*, const FAnimNotifyEvent&)
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override = default;

    virtual void Serialize(FArchive& Ar) override;
    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    virtual void PostEditProperty(const char* PropertyName) override;

    void TickComponent(float DeltaTime) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

    void ResetToBindPose();

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

    void SetAnimInstance(UAnimInstance* InAnimInstance) { AnimInstance = InAnimInstance; }
    UAnimInstance* GetAnimInstance() const { return AnimInstance; }

    void SetAnimationMode(EAnimationMode InAnimationMode);
    EAnimationMode GetAnimationMode() const { return AnimationMode; }

    // 애니메이션
    void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);

    void SetAnimation(UAnimationAsset* NewAnimToPlay);
    UAnimationAsset* GetAnimation() const { return AnimationToPlay; }

    void Play(bool bLooping);
    void Stop();
    void Pause();
    void SetPlayRate(float InPlayRate);
    void SetAnimationPosition(float InTime);

    bool IsPlaying() const { return bPlaying; }
    bool IsLooping() const { return bLooping; }

    // 노티파이 수신 - AnimInstance가 호출해줄 함수
    virtual void HandleAnimNotify(const FAnimNotifyEvent& Notify);
    void ApplyAnimationPose(const FPoseContext& PoseContext);

public:
    FOnAnimNotify OnAnimNotifyDelegate;

private:
    UAnimInstance* AnimInstance = nullptr;
    FString AnimationAssetPath;

    EAnimationMode AnimationMode = EAnimationMode::AnimationBlueprint;
    UAnimationAsset* AnimationToPlay = nullptr;
    bool bPlaying = false;
    bool bLooping = false;
};