#include "RotatingMovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(URotatingMovementComponent, UMovementComponent)
REGISTER_FACTORY(URotatingMovementComponent)

void URotatingMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);

    OutProps.push_back({"Rotation Rate", EPropertyType::Vec3, &RotationRate.X, -360.0f, 360.0f, 1.0f});
    OutProps.push_back({"Pivot Translation", EPropertyType::Vec3, &PivotTranslation.X, 0.0f, 0.0f, 0.1f});
    OutProps.push_back({"Local Space Rotation", EPropertyType::Bool, &bRotationInLocalSpace});
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

    FQuat DeltaQuat = FQuat::MakeFromEuler(RotationRate * DeltaTime);
    DeltaQuat.Normalize();

    // Pivot Offset이 존재하지 않는다면 로컬/월드 공간을 기준으로 한 회전을 수행한다.
    if (PivotTranslation.IsNearlyZero())
    {
        FQuat CurrentQuat = UpdatedComponent->GetRelativeQuat();
        FQuat ResultQuat;

        if (bRotationInLocalSpace)
        {
            // 로컬 공간 기준 회전: 로컬 프레임에서 Delta를 합성 (CurrentQuat * Delta)
            // 표준 수학에서 A*B는 B가 먼저 적용 — DeltaQuat이 로컬 좌표계에서 적용됨
            ResultQuat = (CurrentQuat * DeltaQuat).GetNormalized();
        }
        else
        {
            // 월드 공간 기준 회전: 월드 프레임에서 Delta를 합성 (Delta * CurrentQuat)
            // DeltaQuat이 바깥(월드 좌표계)에서 적용됨 — 짐벌 락 없음
            ResultQuat = (DeltaQuat * CurrentQuat).GetNormalized();
        }

        UpdatedComponent->SetRelativeRotationQuat(ResultQuat);
    }
    else
    {
        FTransform CurrentTransform = UpdatedComponent->GetRelativeTransform();
        FVector CurrentLocation = CurrentTransform.GetTranslation();

        FVector PivotOffset = CurrentTransform.GetRotation().RotateVector(PivotTranslation);
        FVector NewLocation = (CurrentLocation + PivotOffset) - DeltaQuat.RotateVector(PivotOffset);
        UpdatedComponent->SetRelativeLocation(NewLocation);

        // 피벗 회전도 쿼터니언으로 합성 — bRotationInLocalSpace 존중
        FQuat CurrentQuat = UpdatedComponent->GetRelativeQuat();
        FQuat ResultQuat = bRotationInLocalSpace
            ? (CurrentQuat * DeltaQuat).GetNormalized()
            : (DeltaQuat * CurrentQuat).GetNormalized();
        UpdatedComponent->SetRelativeRotationQuat(ResultQuat);
    }
}