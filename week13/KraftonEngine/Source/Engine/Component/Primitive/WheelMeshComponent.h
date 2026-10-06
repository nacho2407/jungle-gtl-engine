#pragma once

#include "Component/Primitive/StaticMeshComponent.h"

#include "Source/Engine/Component/Primitive/WheelMeshComponent.generated.h"

UCLASS()
class UWheelMeshComponent : public UStaticMeshComponent
{
public:
	GENERATED_BODY()

	UWheelMeshComponent() = default;
	~UWheelMeshComponent() override = default;

	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetSuspensionLoad(float InSuspensionLoad, const FVector& InContactNormal, float InWheelRadius, bool bInAir);

	float GetDeformationDepth() const { return DeformationDepth; }
	float GetWheelRadius() const { return WheelRadius; }
	const FVector& GetContactNormal() const { return ContactNormal; }

private:
	UPROPERTY(Edit, Save, Category="Wheel|Deformation", DisplayName="Enable Tire Deformation")
	bool bEnableTireDeformation = true;
	UPROPERTY(Edit, Save, Category="Wheel|Deformation", DisplayName="Reference Load", Min=1.0f, Max=100000.0f, Speed=100.0f)
	float ReferenceLoad = 4000.0f;
	UPROPERTY(Edit, Save, Category="Wheel|Deformation", DisplayName="Max Deformation Depth", Min=0.0f, Max=1.0f, Speed=0.005f)
	float MaxDeformationDepth = 0.08f;

	float WheelRadius = 0.38f;
	float DeformationDepth = 0.0f;
	FVector ContactNormal = FVector(0.0f, 0.0f, 1.0f);
};
