#pragma once

#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
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

    void ClearAnimVariables();

protected:
    USkeletalMeshComponent* OwningComponent = nullptr;

	// animation variable storages(for state machine)
    TMap<FName, float> FloatVariables;
    TMap<FName, bool> BoolVariables;
};
