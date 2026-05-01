#pragma once
#include "Render/Scene/Proxies/SceneProxy.h"

class UShapeComponent;
class FScene;
class UShapeComponent;

class FShapeProxy : public FSceneProxy
{
public:
    FShapeProxy(UShapeComponent* InComponent);
    virtual ~FShapeProxy() = default;

    //virtual void UpdateTransform();

    virtual void VisualizeDebugLineInEditor(FScene& Scene, float DebugScale = 1.0f) const {}
protected:
    UShapeComponent* Owner = nullptr;
};
