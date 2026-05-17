#pragma once

#include "SkeletalMeshComponent.generated.h"

#include "Animation/AnimationTypes.h"
#include "Component/SkinnedMeshComponent.h"

class UAnimInstance;
class UAnimSingleNodeInstance;
class UAnimationAsset;
struct FTransform;

UCLASS()
class USkeletalMeshComponent : public USkinnedMeshComponent
{
    GENERATED_BODY_USkeletalMeshComponent()
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override;

    void TickComponent(float DeltaTime) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

    void SetAnimationMode(EAnimationMode InMode);
    EAnimationMode GetAnimationMode() const;
    void SetAnimInstanceClass(const FString& InClassName);
    UAnimInstance* GetAnimInstance() const;
    void RecreateAnimInstance();
    bool ApplyAnimationLocalPose(const TArray<FTransform>& LocalPose);
    bool RefreshAnimationPose();

    void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);
    void SetAnimation(UAnimationAsset* NewAnimToPlay);
    UAnimationAsset* GetAnimation() const;
    void Play();
    void Pause();
    void Stop();
    void SetPosition(float TimeSeconds, bool bFireNotifies = false);
    float GetPosition() const;
    void SetPlayRate(float InPlayRate);
    float GetPlayRate() const;
    void SetLooping(bool bInLooping);
    bool IsLooping() const;
    bool IsPlaying() const;
    float GetPlayLength() const;

    bool SetAnimSequence(const FString& SourceFbxPath, const FString& AnimStackName = FString());
    void SetAnimationTime(float Time);
    void TickAnimation(float DeltaTime);
    void PlayAnim(bool bLoop);
    void StopAnim();
    float GetAnimationTime() const { return GetPosition(); }
    bool IsAnimPlaying() const { return IsPlaying(); }

    void ResetToBindPose();

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

private:
    void DestroyAnimInstance();
    UAnimSingleNodeInstance* GetSingleNodeInstance() const;
    UAnimSingleNodeInstance* EnsureSingleNodeInstance();

private:
    EAnimationMode AnimationMode = EAnimationMode::None;
    FString AnimInstanceClassName;
    UAnimInstance* AnimInstance = nullptr;
};
