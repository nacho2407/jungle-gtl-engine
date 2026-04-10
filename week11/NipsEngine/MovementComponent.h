#pragma once

#include "Engine/Component/SceneComponent.h"

/* UE MovementComponent를 참고하여 구현했습니다. */

class UMovementComponent : public UActorComponent
{
	virtual void TickComponent(float DeltaTime) override;

	void SafeMoveUpdatedComponent(); // 충돌을 고려하며 UpdatedComponent를 이동시킨다. 장애물을 만나면 멈추거나 FHitResult를 반환한다.
	void SlideAlongSurface(); // 벽, 경사면에 부딪쳤을 때 멈추지 않고 미끄러지듯 이동하도록 계산한다.
	void ConsumeInputVector(); // Pawn 계열에서 사용 (입력 시스템을 통해 이동 입력 벡터를 가져오고, 값을 0으로 초기화한다.)

	// 최대 이동 속도 관련 함수 (하위 클래스에서 각기 로직에 따라 오버라이딩하여 사용한다.)
	virtual void GetMaxSpeed(); 
	virtual bool IsExceedingMaxSpeed();

	void StopMovementImmediately() { Velocity = FVector(0.0f, 0.0f, 0.0f); }

private:
	USceneComponent* UpdatedComponent = nullptr;
	FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
	FVector PlaneConstraintNormal = FVector(0.0f, 0.f, 1.0f); // 이동을 특정 평면으로 제한할 때 사용하는 법선 벡터

	bool bUpdateOnlyIfRendered = false; // 화면에 보일 때만 이동 계산을 수행할지 결정
	bool bConstrainToPlane = false;     // 이동을 지정된 평면 내로 제한할지 결정
};