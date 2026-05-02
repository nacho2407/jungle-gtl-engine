#pragma once
#include "Engine/Component/PrimitiveComponent.h"

class FShapeProxy;

class UShapeComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)
    UShapeComponent();
    ~UShapeComponent();

    //void BeginPlay() override;

    //void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    //void PostEditProperty(const char* PropertyName) override;
    void Serialize(FArchive& Ar) override;
    void PostDuplicate() override;

    virtual void OnTransformDirty() override;

	FColor GetColor(){  return ShapeColor;  }

	virtual void OnOwnerChanged() override;

protected:
    FColor ShapeColor = { 0, 255, 0, 1 };
    bool bDrawOnlyIfSelected;
};
