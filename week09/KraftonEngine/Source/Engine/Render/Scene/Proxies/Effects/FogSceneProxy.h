#pragma once

#include "Render/Execute/Passes/Scene/FogParams.h"
#include "Render/Scene/Proxies/Effects/SceneEffectProxy.h"

class UHeightFogComponent;

/*
    FFogSceneProxy??HeightFogComponent???�더 ?�라미터�?Scene effect 계층??보�??�니??
    ?�재??HeightFogPass가 �?번째 fog ?�록?�의 ?�라미터�??�어 ?�용?�니??
*/
class FFogSceneProxy : public FSceneEffectProxy
{
public:
    FFogSceneProxy(const UHeightFogComponent* InOwner, const FFogParams& InParams)
        : Owner(InOwner), Params(InParams)
    {
    }

    void UpdateParams(const FFogParams& InParams)
    {
        Params = InParams;
        DirtyFlags = EDirtyFlag::All;
    }

    const UHeightFogComponent* GetOwner() const { return Owner; }
    const FFogParams& GetFogParams() const { return Params; }

private:
    const UHeightFogComponent* Owner = nullptr;
    FFogParams Params;
};
