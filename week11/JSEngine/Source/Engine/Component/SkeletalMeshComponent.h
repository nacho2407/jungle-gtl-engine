#pragma once

#include "SkeletalMeshComponent.generated.h"

#include "Animation/AnimationTypes.h"
#include "Component/SkinnedMeshComponent.h"

class UAnimInstance;
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

    void ResetToBindPose();

    void SetBoneLocalTransform(int32 BoneIndex, const FMatrix& NewLocalTransform);
    const FMatrix& GetBoneLocalTransform(int32 BoneIndex) const;

    FMatrix GetBoneGlobalTransform(int32 BoneIndex) const;
    void SetBoneGlobalTransform(int32 BoneIndex, const FMatrix& NewGlobalTransform);

private:
    void DestroyAnimInstance();

private:
    EAnimationMode AnimationMode = EAnimationMode::None;
    FString AnimInstanceClassName;
    UAnimInstance* AnimInstance = nullptr;
};
