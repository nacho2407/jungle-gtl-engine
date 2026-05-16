#pragma once
#include "BillboardComponent.h"

class UCylindricalBillboardComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UCylindricalBillboardComponent, UBillboardComponent)
	static void RegisterProperties(UClass* Class);

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction);
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetBillboardAxis(const FVector& Axis) { BillboardAxis = Axis; }
	FVector GetBillboardAxis() const { return BillboardAxis; }

protected:
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="BillboardAxis")
	FVector BillboardAxis = FVector(0, 0, 1);
};
