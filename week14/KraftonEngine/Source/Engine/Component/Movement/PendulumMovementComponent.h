#pragma once

#include "MovementComponent.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

#include "Source/Engine/Component/Movement/PendulumMovementComponent.generated.h"
// 그네(진자)처럼 지정 축을 중심으로 왕복 회전하는 이동 컴포넌트
// angle(t) = Amplitude * sin(2π * Frequency * t + Phase)
UCLASS()
class UPendulumMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY()
	UPendulumMovementComponent() = default;
	~UPendulumMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
private:
	// 회전 축 (로컬 기준, 정규화됨). 기본값 Y축 = 좌우 흔들림
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Swing Axis")
	FVector Axis = FVector(0.0f, 1.0f, 0.0f);

	FQuat InitialRelativeRotation;

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Amplitude (deg)", Min=0.0f, Max=180.0f, Speed=0.5f)
	float Amplitude    = 30.0f;	// 최대 회전 각도 (도)
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Frequency (Hz)", Min=0.01f, Max=10.0f, Speed=0.01f)
	float Frequency    = 0.5f;	// 초당 왕복 횟수 (Hz)
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Phase (deg)", Min=0.0f, Max=360.0f, Speed=1.0f)
	float Phase        = 0.0f;	// 초기 위상 (도)
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Angle Offset (deg)", Min=-180.0f, Max=180.0f, Speed=0.5f)
	float AngleOffset  = 0.0f;	// 기본 회전 오프셋 (도) — 진동 중심을 기울임

	float ElapsedTime = 0.0f;	// 누적 시간 (직렬화 제외)
};
