#pragma once
#include "PrimitiveComponent.h"

class UShapeComponent : public UPrimitiveComponent
{
    FColor ShapeColor;
    bool bDrawOnlyIfSelected;
};