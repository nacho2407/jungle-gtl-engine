#pragma once
#include "BillboardComponent.h"


#include "Source/Engine/Component/CylindricalBillboardComponent.generated.h"

UCLASS()
class UCylindricalBillboardComponent : public UBillboardComponent
{
public:
	GENERATED_BODY()
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction);
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetBillboardAxis(const FVector& Axis) { BillboardAxis = Axis; }
	FVector GetBillboardAxis() const { return BillboardAxis; }

protected:
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="BillboardAxis")
	FVector BillboardAxis = FVector(0, 0, 1);
};
