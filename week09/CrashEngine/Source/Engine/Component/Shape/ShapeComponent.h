#pragma once
#include "Engine/Component/PrimitiveComponent.h"

class FShapeProxy;

class UShapeComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)
    UShapeComponent();
    ~UShapeComponent();

    // void BeginPlay() override;

    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void PostDuplicate() override;

    virtual void OnTransformDirty() override;

    FColor GetColor() { return ShapeColor; }

    virtual void OnOwnerChanged() override;

protected:
    FColor ShapeColor = { 0, 255, 0, 1 };
    bool bDrawOnlyIfSelected;	//해당 컴포넌트를 클릭(선택)했을 때만 화면에 그릴지 여부
};
