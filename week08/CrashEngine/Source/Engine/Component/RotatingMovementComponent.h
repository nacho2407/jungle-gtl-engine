// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "MovementComponent.h"
#include "Math/Rotator.h"

// URotatingMovementComponent 컴포넌트이다.
class URotatingMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(URotatingMovementComponent, UMovementComponent)

    URotatingMovementComponent() = default;
    ~URotatingMovementComponent() override = default;

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;

private:
    FRotator RotationRate = FRotator(0.0f, 90.0f, 0.0f);
};
