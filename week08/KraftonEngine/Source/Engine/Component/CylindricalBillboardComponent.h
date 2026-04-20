#pragma once
#include "BillboardComponent.h"

class UCylindricalBillboardComponent : public UBillboardComponent
{
public:
	DECLARE_CLASS(UCylindricalBillboardComponent, UBillboardComponent)

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction);
	FMatrix ComputeBillboardMatrix(const FVector& CameraForward) const;
	FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetBillboardAxis(const FVector& Axis) { BillboardAxis = Axis; }
	FVector GetBillboardAxis() const { return BillboardAxis; }

protected:
	FVector BillboardAxis = FVector(0, 0, 1);
};
