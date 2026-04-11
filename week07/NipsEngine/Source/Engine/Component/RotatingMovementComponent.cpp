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
	NewComp->SetTransient(this->IsTransient());
	NewComp->SetEditorOnly(this->IsEditorOnly());
    NewComp->SetOwner(nullptr);
	
	// UpdatedComponent는 Actor 단에서 맵핑해줍니다.

    NewComp->RotationRate = this->RotationRate;
    NewComp->PivotTranslation = this->PivotTranslation;
    NewComp->bRotationInLocalSpace = this->bRotationInLocalSpace;

    return NewComp;
}

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Rotation Rate", EPropertyType::Vec3, &RotationRate.X, -360.0f, 360.0f, 1.0f });
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

	FRotator DeltaRotation = FRotator::MakeFromEuler(RotationRate * DeltaTime);

	// Pivot Offset이 존재하지 않는다면 로컬/월드 공간을 기준으로 한 회전을 수행한다.
	if (PivotTranslation.IsNearlyZero())
	{
		if (bRotationInLocalSpace)
		{
			// 로컬 공간 기준 회전: RelativeRotation에 직접 delta를 더한다
			FVector CurrentRot = UpdatedComponent->GetRelativeRotation();
			CurrentRot += DeltaRotation.Euler();
			UpdatedComponent->SetRelativeRotation(CurrentRot);
		}
		else
		{
			// 월드 공간 기준 회전: 월드 회전에 delta를 적용한 뒤 로컬로 환산한다
			FQuat CurrentWorldQuat = UpdatedComponent->GetRelativeQuat();
			FQuat DeltaQuat        = DeltaRotation.Quaternion();
			UpdatedComponent->SetRelativeRotationQuat((DeltaQuat * CurrentWorldQuat).GetNormalized());
		}
	}
	else
	{
		FTransform CurrentTransform = UpdatedComponent->GetRelativeTransform();
		FVector CurrentLocation = CurrentTransform.GetTranslation();

		FQuat RotationQuat = DeltaRotation.Quaternion();
		FVector PivotOffset = CurrentTransform.GetRotation().RotateVector(PivotTranslation);

		FVector NewLocation = (CurrentLocation + PivotOffset) - RotationQuat.RotateVector(PivotOffset);
		UpdatedComponent->SetRelativeLocation(NewLocation);

		FVector NewRot = UpdatedComponent->GetRelativeRotation() + DeltaRotation.Euler();
		UpdatedComponent->SetRelativeRotation(NewRot);
	}
}