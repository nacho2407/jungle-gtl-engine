#pragma once

#include "Component/SkinnedMeshComponent.h"

class UAnimSequence;

/**
 * @brief Unreal Engine 스타일에서는 skinned mesh가 skeleton을 이용하는 mesh를 표현하고,
 *        skeletal mesh는 실제로 actor에 붙어서 애니메이션을 붙일 수 있는 component로 사용되고 있으므로
 *        USkeletalMeshComponent 또한 해당 방식대로 우선은 얇게 유지.
 *        핵심 로직들은 대부분 USkinnedMeshComponent로 옮겼습니다.
 */
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override = default;

    void TickComponent(float DeltaTime) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

    void ResetToBindPose();

    void SetAnimation(UAnimSequence* InSequence);
    bool SetAnimSequence(const FString& SourceFbxPath, const FString& AnimStackName = FString());
    void SetAnimationTime(float Time);
    void TickAnimation(float DeltaTime);
    void PlayAnim(bool bLoop);
    void StopAnim();
    UAnimSequence* GetAnimation() const { return AnimationSequence; }
    float GetAnimationTime() const { return AnimationTime; }
    bool IsAnimPlaying() const { return bAnimationPlaying; }

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

private:
    bool ApplyAnimationPose();

private:
    UAnimSequence* AnimationSequence = nullptr;
    float AnimationTime = 0.0f;
    float AnimationPlayRate = 1.0f;
    bool bAnimationPlaying = false;
    bool bAnimationLooping = false;
};
