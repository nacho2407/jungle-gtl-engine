#pragma once

#include "Core/Containers/Array.h"
#include "Engine/Geometry/Transform.h"
#include "Object/Object.h"

class USkeletalMeshComponent;

class UAnimInstance : public UObject
{
public:
    DECLARE_CLASS(UAnimInstance, UObject)

    UAnimInstance() = default;
    ~UAnimInstance() override = default;

    void Initialize(USkeletalMeshComponent* InOwningComponent);

    virtual void NativeInitializeAnimation();
    virtual void NativeUpdateAnimation(float DeltaSeconds);
    virtual bool EvaluateAnimation(TArray<FTransform>& OutLocalPose);

    USkeletalMeshComponent* GetOwningComponent() const;

	/**
	 * @note Unreal Engine style alias
	 */
    USkeletalMeshComponent* GetSkelMeshComponent() const;

protected:
    USkeletalMeshComponent* OwningComponent = nullptr;
};
