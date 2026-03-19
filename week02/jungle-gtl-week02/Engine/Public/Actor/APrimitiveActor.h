#pragma once

#include "Actor/AActor.h"

class UPrimitiveComponent;

class APrimitiveActor : public AActor
{
DECLARE_UClass(APrimitiveActor, AActor)

public:
    APrimitiveActor();
    virtual ~APrimitiveActor() override;

public:
    UPrimitiveComponent* GetPrimitiveComponent() const;

protected:
    void SetPrimitiveComponent(UPrimitiveComponent* InPrimitiveComponent);

protected:
    UPrimitiveComponent* PrimitiveComponent = nullptr;
};