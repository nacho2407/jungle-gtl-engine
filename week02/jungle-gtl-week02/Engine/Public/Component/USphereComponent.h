#pragma once

#include "Component/UPrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
    DECLARE_UClass(USphereComponent, UPrimitiveComponent)

public:
    USphereComponent();
    virtual ~USphereComponent();

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime) override;
    virtual const char* GetObjClassName() const override;

public:
    void SetRadius(float InRadius);
    float GetRadius() const;

private:
    float Radius;
};