#pragma once
#include "ShapeProxy.h"

class UBoxComponent;

class FBoxProxy : public FShapeProxy
{
public:
    FBoxProxy(UBoxComponent* InComponent);
    //void UpdateTransform() override;
    void VisualizeDebugLineInEditor(FScene& Scene, float DebugScale = 1.0f) const override;
};
