#include "AnimInstance.h"

DEFINE_CLASS(UAnimInstance, UObject)

void UAnimInstance::Serialize(FArchive& Ar)
{
    UObject::Serialize(Ar);
}

void UAnimInstance::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UObject::GetEditableProperties(OutProps);
}

void UAnimInstance::PostEditProperty(const char* PropertyName)
{
    UObject::PostEditProperty(PropertyName);
}

void UAnimInstance::Initialize(USkeletalMeshComponent* InOwnerComponent)
{
	OwnerComponent = InOwnerComponent;
}