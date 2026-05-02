#pragma once

#include "Component/DecalComponent.h"

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
	bool IsWashed() const { return bWashed; }

	static bool WashFirstHitDirt(UWorld* World, const FVector& Start, const FVector& Direction, float MaxDistance);

protected:
	bool ShouldReceivePrimitive(UPrimitiveComponent* PrimitiveComp) const override;

private:
	FVector DirtColor = FVector(0.34f, 0.23f, 0.12f);
	float CurrentAlpha = 0.95f;
	float AlphaDecreasePerHit = 0.001f;
	bool bWashed = false;
};
