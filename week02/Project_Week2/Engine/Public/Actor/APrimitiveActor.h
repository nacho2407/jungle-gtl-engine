#pragma once

#include "Actor/AActor.h"

class UPrimitiveComponent;

class APrimitiveActor : public AActor
{
public:
    virtual ~APrimitiveActor() = default;

public:
    UPrimitiveComponent* PrimitiveComponent = nullptr;
};