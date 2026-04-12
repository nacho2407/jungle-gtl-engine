#pragma once

#include "ActorComponent.h"
#include "Core/CollisionTypes.h"

class USceneComponent;

/**
 * @brief 이동 컴포넌트의 기반이 되는 추상 클래스
 */
class UMovementComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UMovementComponent, UActorComponent)

    virtual void TickComponent(float DeltaTime) override = 0;

    void SetUpdatedComponent(USceneComponent* InComponent);
    USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }

    virtual void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	// 충돌을 고려하며 UpdatedComponent를 Delta만큼 이동시킨다.
	// bool SafeMoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit);

	// 벽·경사면에 부딪혔을 때 멈추지 않고 표면을 따라 미끄러진다.
    // void SlideAlongSurface(const FVector& Delta, const FHitResult& Hit, float RemainingTimeFraction);

	// 월드 방향으로 이동 입력을 누적한다. (Pawn 입력이 추가되었을 경우 사용합니다.)
    void AddInputVector(const FVector& WorldDirection, float Scale = 1.0f);

	// 누적된 입력 벡터를 반환하고, 내부 값을 0으로 초기화한다.
    FVector ConsumeInputVector();

    virtual float GetMaxSpeed() const = 0;
	// virtual void SetMaxSpeed(float ) const = 0;
    virtual bool IsExceedingMaxSpeed(float MaxSpeed) const;

    FVector GetVelocity() const { return Velocity; }
    void    SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }

	FVector GetPendingInputVector() const { return PendingInputVector; }
	void    SetPendingInputVector(const FVector InVector) { PendingInputVector = InVector; }

    FVector GetPlaneConstraintNormal() const { return PlaneConstraintNormal; }
    void    SetPlaneConstraintNormal(const FVector& InNormal) { PlaneConstraintNormal = InNormal; }

	// bConstrainToPlane 값에 따라 컴포넌트의 방향 벡터, 위치 벡터를 평면에 제약해 반환합니다.
    FVector ConstrainDirectionToPlane(const FVector& Direction) const;
    FVector ConstrainLocationToPlane(const FVector& Location) const;

	// 이동속도를 즉시 0으로 만듭니다.
    void StopMovementImmediately() { Velocity = FVector::ZeroVector; }

protected:
	// UpdatedComponent를 Delta만큼 이동시킵니다.
    void MoveUpdatedComponent(const FVector& Delta);

protected:
    USceneComponent* UpdatedComponent = nullptr;
	FVector Velocity = FVector(-1.0f, 0.0f, 1.0f);
    FVector PendingInputVector = FVector::ZeroVector;          // 추후 플레이어 입력을 처리할 때 사용되는 벡터
    FVector PlaneConstraintNormal = FVector(0.0f, 0.0f, 1.0f); // 이동을 특정 평면으로 제한하는 법선 벡터

    bool bUpdateOnlyIfRendered = false; // 화면에 보일 때만 이동 계산을 수행할지 결정
    bool bConstrainToPlane = false;     // 이동을 지정된 평면 내로 제한할 지 결정
};
