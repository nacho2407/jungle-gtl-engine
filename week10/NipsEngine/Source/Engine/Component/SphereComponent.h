#pragma once
#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
    float GetSphereRadius() const { return SphereRadius; }

private:
    float SphereRadius;
};
