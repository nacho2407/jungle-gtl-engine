#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/Set.h"
#include "Animation/AnimationTypes.h"
#include "Engine/Geometry/Transform.h"
#include "Object/Object.h"

class USkeletalMeshComponent;

/**
 * @brief animation runtime 로직의 base class
 */
class UAnimInstance : public UObject
{
public:
    DECLARE_CLASS(UAnimInstance, UObject)

    UAnimInstance() = default;
    ~UAnimInstance() override = default;

    void Initialize(USkeletalMeshComponent* InOwningComponent);
    void Uninitialize();

    virtual void NativeInitializeAnimation();
    virtual void NativeUninitializeAnimation();
    virtual void NativeUpdateAnimation(float DeltaSeconds);
    virtual bool EvaluateAnimation(TArray<FTransform>& OutLocalPose);

    USkeletalMeshComponent* GetOwningComponent() const;

	/**
	 * @note Unreal Engine style alias
	 */
    USkeletalMeshComponent* GetSkelMeshComponent() const;

    void SetAnimVariableFloat(const FName& Name, float Value);
    bool GetAnimVariableFloat(const FName& Name, float& OutValue) const;
    float GetAnimVariableFloatOrDefault(const FName& Name, float DefaultValue = 0.0f) const;

    void SetAnimVariableBool(const FName& Name, bool Value);
    bool GetAnimVariableBool(const FName& Name, bool& OutValue) const;
    bool GetAnimVariableBoolOrDefault(const FName& Name, bool DefaultValue = false) const;

    void SetAnimTrigger(const FName& Name);
    void ResetAnimTrigger(const FName& Name);
    bool IsAnimTriggerSet(const FName& Name) const;
    bool ConsumeAnimTrigger(const FName& Name);
    void ClearAnimTriggers();

    void ClearAnimVariables();

    void SetRootMotionMode(ERootMotionMode InMode);
    ERootMotionMode GetRootMotionMode() const;
    const FRootMotionDelta& GetLastExtractedRootMotion() const;
    void SetRootMotionBoneIndex(int32 InBoneIndex);
    int32 GetRootMotionBoneIndex() const;
    void SetRootMotionBoneName(const FName& InBoneName);
    FName GetRootMotionBoneName() const;

protected:
    void ProcessRootMotion(TArray<FTransform>& InOutLocalPose, bool bResetDelta = false);
    void ClearRootMotionState();

    USkeletalMeshComponent* OwningComponent = nullptr;

	// animation variable storages(for state machine)
    TMap<FName, float> FloatVariables;
    TMap<FName, bool> BoolVariables;
    TSet<FName, TDefaultMapHash<FName>> TriggerVariables;

    ERootMotionMode RootMotionMode = ERootMotionMode::Ignore;
    FRootMotionDelta LastExtractedRootMotion;
    int32 RootMotionBoneIndex = -1;
    FName RootMotionBoneName;

    FTransform PreviousRootTransform = FTransform::Identity;
    bool bHasPreviousRootTransform = false;
};
