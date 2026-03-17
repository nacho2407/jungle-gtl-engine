#pragma once

#include "Actor/AActor.h"

class UPrimitiveComponent;

class APrimitiveActor : public AActor
{
DECLARE_ROOT_UClass(APrimitiveActor)

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