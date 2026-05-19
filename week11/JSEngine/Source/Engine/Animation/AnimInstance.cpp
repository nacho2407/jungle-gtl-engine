#include "Animation/AnimInstance.h"

DEFINE_CLASS(UAnimInstance, UObject)

namespace
{
    bool IsValidAnimVariableName(const FName& Name)
    {
        return Name.IsValid() && Name != FName::None;
    }
}

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwningComponent)
{
    OwningComponent = InOwningComponent;
    NativeInitializeAnimation();
}

void UAnimInstance::Uninitialize()
{
    if (!OwningComponent)
    {
        return;
    }

    NativeUninitializeAnimation();
    OwningComponent = nullptr;
}

void UAnimInstance::NativeInitializeAnimation()
{
}

void UAnimInstance::NativeUninitializeAnimation()
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

void UAnimInstance::SetAnimVariableFloat(const FName& Name, float Value)
{
    if (!IsValidAnimVariableName(Name))
    {
        return;
    }

    FloatVariables[Name] = Value;
}

bool UAnimInstance::GetAnimVariableFloat(const FName& Name, float& OutValue) const
{
    if (!IsValidAnimVariableName(Name))
    {
        return false;
    }

    const auto It = FloatVariables.find(Name);
    if (It == FloatVariables.end())
    {
        return false;
    }

    OutValue = It->second;
    return true;
}

float UAnimInstance::GetAnimVariableFloatOrDefault(const FName& Name, float DefaultValue) const
{
    float Value = DefaultValue;
    return GetAnimVariableFloat(Name, Value) ? Value : DefaultValue;
}

void UAnimInstance::SetAnimVariableBool(const FName& Name, bool Value)
{
    if (!IsValidAnimVariableName(Name))
    {
        return;
    }

    BoolVariables[Name] = Value;
}

bool UAnimInstance::GetAnimVariableBool(const FName& Name, bool& OutValue) const
{
    if (!IsValidAnimVariableName(Name))
    {
        return false;
    }

    const auto It = BoolVariables.find(Name);
    if (It == BoolVariables.end())
    {
        return false;
    }

    OutValue = It->second;
    return true;
}

bool UAnimInstance::GetAnimVariableBoolOrDefault(const FName& Name, bool DefaultValue) const
{
    bool Value = DefaultValue;
    return GetAnimVariableBool(Name, Value) ? Value : DefaultValue;
}

void UAnimInstance::ClearAnimVariables()
{
    FloatVariables.clear();
    BoolVariables.clear();
}
