// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "BillboardComponent.h"

// UCylindricalBillboardComponent 컴포넌트이다.
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
