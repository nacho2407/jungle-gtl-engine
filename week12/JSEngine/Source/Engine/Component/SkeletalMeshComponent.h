#pragma once

#include "Component/SkinnedMeshComponent.h"

class UAnimationAsset;

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
    DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

    USkeletalMeshComponent() = default;
    ~USkeletalMeshComponent() override = default;

    void TickComponent(float DeltaTime) override;

    EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_SkeletalMesh; }

    void ResetToBindPose();

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

    void SetAnimationMode(EAnimationMode InAnimationMode);
    EAnimationMode GetAnimationMode() const { return AnimationMode; }

    void SetAnimation(UAnimationAsset* NewAnimation);
    UAnimationAsset* GetAnimation() const { return AnimationToPlay; }

    void Play(bool bLooping);
    void Stop();
    bool IsPlaying() const { return bPlaying; }
    bool IsLooping() const { return bLooping; }

    void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);

private:
    EAnimationMode AnimationMode = EAnimationMode::AnimationBlueprint;
    UAnimationAsset* AnimationToPlay = nullptr;
    bool bPlaying = false;
    bool bLooping = false;
};
