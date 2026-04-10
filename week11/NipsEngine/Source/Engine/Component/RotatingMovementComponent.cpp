#include "RotatingMovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(URotatingMovementComponent, UMovementComponent)
REGISTER_FACTORY(URotatingMovementComponent)

URotatingMovementComponent* URotatingMovementComponent::Duplicate()
{
    URotatingMovementComponent* NewComp = UObjectManager::Get().CreateObject<URotatingMovementComponent>();
    
	NewComp->SetActive(this->IsActive());
	NewComp->SetAutoActivate(this->IsAutoActivate());
	NewComp->SetComponentTickEnabled(this->IsComponentTickEnabled());
    NewComp->SetOwner(nullptr);

    NewComp->RotationRate = this->RotationRate;
    NewComp->PivotTranslation = this->PivotTranslation;
    NewComp->bRotationInLocalSpace = this->bRotationInLocalSpace;

    return NewComp;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    OutProps.push_back({ "Rotation Rate", EPropertyType::Vec3, &RotationRate.Pitch, -360.0f, 360.0f, 1.0f });
    OutProps.push_back({ "Pivot Translation", EPropertyType::Vec3, &PivotTranslation.X, 0.0f, 0.0f, 0.1f });
    OutProps.push_back({ "Local Space Rotation", EPropertyType::Bool, &bRotationInLocalSpace });
}

void URotatingMovementComponent::TickComponent(float DeltaTime)
{
	if (UpdatedComponent == nullptr) 
	{
		return;
	}

	// Primitive Component이고, 화면에 보일 때만 렌더링 업데이트 옵션이 켜져 있는 경우 예외처리
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (bUpdateOnlyIfRendered && PrimitiveComponent && !PrimitiveComponent->IsVisible())
	{
		return;
	}

	FRotator DeltaRotation = FRotator(RotationRate.Pitch * DeltaTime, RotationRate.Yaw * DeltaTime, RotationRate.Roll * DeltaTime);

	// Pivot Offset이 존재하지 않는다면 로컬 공간, 월드 공간을 기준으로 한 회전을 수행한다.
	if (PivotTranslation.IsNearlyZero())
	{
		if (bRotationInLocalSpace)
        {
            // 로컬 공간을 기준으로 회전 추가
            FVector CurrentRot = UpdatedComponent->GetRelativeRotation();
            CurrentRot += DeltaRotation.Euler();
            UpdatedComponent->SetRelativeRotation(CurrentRot);
        }
        else
        {
            // 월드 공간을 기준으로 회전
            FVector CurrentRot = UpdatedComponent->GetRelativeRotation();
            CurrentRot += DeltaRotation.Euler();
            UpdatedComponent->SetRelativeRotation(CurrentRot);
        }
	}
	else
	{
		// 피벗 오프셋이 존재할 때는 [피벗 이동] -> [회전] -> [위치 복원] 과정을 수행한다.
        FTransform CurrentTransform = UpdatedComponent->GetRelativeTransform();
        FVector CurrentLocation = CurrentTransform.GetTranslation();
        
        // 피벗을 원점으로 하여 회전 행렬/쿼터니언을 적용하기 위한 오프셋 연산
        FQuat RotationQuat = DeltaRotation.Quaternion();
        FVector PivotOffset = CurrentTransform.GetRotation().RotateVector(PivotTranslation);
        
        FVector NewLocation = (CurrentLocation - PivotOffset) + RotationQuat.RotateVector(PivotOffset);
        UpdatedComponent->SetRelativeLocation(NewLocation);
        
        FVector NewRot = UpdatedComponent->GetRelativeRotation() + DeltaRotation.Euler();
        UpdatedComponent->SetRelativeRotation(NewRot);
	}
}