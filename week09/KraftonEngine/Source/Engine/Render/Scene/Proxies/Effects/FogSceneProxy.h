#pragma once

#include "Render/Passes/Scene/FogParams.h"
#include "Render/Scene/Proxies/Effects/SceneEffectProxy.h"

class UHeightFogComponent;

/*
    FFogSceneProxy는 HeightFogComponent의 렌더 파라미터를 Scene effect 계층에 보관합니다.
    현재는 HeightFogPass가 첫 번째 fog 프록시의 파라미터를 읽어 사용합니다.
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
