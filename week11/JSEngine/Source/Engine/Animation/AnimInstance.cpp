#include "Animation/AnimInstance.h"

DEFINE_CLASS(UAnimInstance, UObject)

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwningComponent)
{
    OwningComponent = InOwningComponent;
    NativeInitializeAnimation();
}

void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    (void)DeltaSeconds;
}

bool UAnimInstance::EvaluateAnimation(TArray<FTransform>& OutLocalPose)
{
    OutLocalPose.clear();
    return false;
}

USkeletalMeshComponent* UAnimInstance::GetOwningComponent() const
{
    return OwningComponent;
}

USkeletalMeshComponent* UAnimInstance::GetSkelMeshComponent() const
{
    return OwningComponent;
}
