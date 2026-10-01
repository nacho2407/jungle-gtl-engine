// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Component/MovementComponent.h"
#include "Core/CollisionTypes.h"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"

// EProjectileHitBehavior는 컴포넌트 처리에서 사용할 선택지를 정의합니다.
enum class EProjectileHitBehavior : int32
{
    Stop = 0,
    Bounce = 1,
    Destroy = 2,
};

// UProjectileMovementComponent 컴포넌트이다.
class UProjectileMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(UProjectileMovementComponent, UMovementComponent)

    UProjectileMovementComponent() = default;
    ~UProjectileMovementComponent() override = default;

    void BeginPlay() override;
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void Serialize(FArchive& Ar) override;
    void ContributeSelectedVisuals(FScene& Scene) const override;

    void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
    const FVector& GetVelocity() const { return Velocity; }
    void SetInitialSpeed(float InInitialSpeed) { InitialSpeed = InInitialSpeed; }
    float GetInitialSpeed() const { return InitialSpeed; }
    float GetMaxSpeed() const { return MaxSpeed; }
    FVector GetPreviewVelocity() const;
    void StopSimulating();

protected:
    FVector ComputeEffectiveVelocity() const;
    virtual EProjectileHitBehavior GetHitBehavior() const;
    virtual bool HandleBlockingHit(USceneComponent* UpdatedSceneComponent, const FVector& CurrentLocation, const FVector& MoveDelta, const FHitResult& HitResult);

    FVector Velocity = FVector(0.0f, 0.0f, 0.0f);
    float InitialSpeed = 10.0f;
    float MaxSpeed = 100.0f;
};
