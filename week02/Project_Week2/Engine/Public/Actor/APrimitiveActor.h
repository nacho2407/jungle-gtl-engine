#pragma once

#include "Actor/AActor.h"

class UPrimitiveComponent;

class APrimitiveActor : public AActor
{
public:
    APrimitiveActor();
    //APrimitiveActor(const FUObjectInitializer& ObjectInitializer);
    virtual ~APrimitiveActor() override;

public:
    virtual const char* GetObjClassName() const override;

public:
    UPrimitiveComponent* GetPrimitiveComponent() const;

protected:
    void SetPrimitiveComponent(UPrimitiveComponent* InPrimitiveComponent);

protected:
    UPrimitiveComponent* PrimitiveComponent = nullptr;
};