// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "MovementComponent.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

// UPendulumMovementComponent 컴포넌트이다.
class UPendulumMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(UPendulumMovementComponent, UMovementComponent)

    UPendulumMovementComponent() = default;
    ~UPendulumMovementComponent() override = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;

private:
    // 회전 축 (로컬 기준, 정규화됨). 기본값 Y축 = 좌우 흔들림
    FVector Axis = FVector(0.0f, 1.0f, 0.0f);

    FQuat InitialRelativeRotation;

    float Amplitude = 30.0f;  // 최대 회전 각도 (도)
    float Frequency = 0.5f;   // 초당 왕복 횟수 (Hz)
    float Phase = 0.0f;       // 초기 위상 (도)
    float AngleOffset = 0.0f; // 기본 회전 오프셋 (도) — 진동 중심을 기울임

    float ElapsedTime = 0.0f; // 누적 시간 (직렬화 제외)
};
