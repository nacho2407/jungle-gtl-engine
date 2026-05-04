#pragma once

#include "Component/DecalComponent.h"

class AActor;
class UStaticMeshComponent;
class UWorld;

class UDirtComponent : public UDecalComponent
{
public:
	DECLARE_CLASS(UDirtComponent, UDecalComponent)

	UDirtComponent();
	~UDirtComponent() override = default;

	bool TryWashByRay(const FRay& Ray, float MaxDistance, float& OutDistance);
	void ApplyWashHit();
	void WashOff();
	void ResetWash();   // bWashed=false 로 되돌리고 alpha/visibility 초기화 (CarWash 페이즈 replay 용)
	bool IsWashed() const { return bWashed; }

	static bool FireCarWashRay(AActor& Actor);
	static void SetCarWashStreamVisible(AActor& Actor, bool bVisible);
	static bool IsCarWashStreamVisible(AActor& Actor);
	static bool AreAllDirtComponentsWashed(AActor& Actor);
	static void ResetAllOnActor(AActor& Actor);   // Actor 의 모든 UDirtComponent ResetWash
	static bool WashFirstHitDirt(UWorld* World, const FVector& Start, const FVector& Direction, float MaxDistance);

protected:
	bool ShouldReceivePrimitive(UPrimitiveComponent* PrimitiveComp) const override;

private:
	static UStaticMeshComponent* FindWaterGunComponent(AActor& Actor);
	static UStaticMeshComponent* FindWaterStreamComponent(AActor& Actor);

	FVector DirtColor = FVector(0.34f, 0.23f, 0.12f);
	float CurrentAlpha = 0.95f;
	float InitialAlpha = 0.95f;          // ResetWash 시 복원 기준
	float AlphaDecreasePerHit = 0.003f;
	bool bWashed = false;
};
