#pragma once

#include "Render/Scene/Proxies/SceneProxy.h"

// FSceneEffectProxy는 렌더 영역의 핵심 동작을 담당합니다.
class FSceneEffectProxy : public FSceneProxy
{
public:
    virtual ~FSceneEffectProxy() = default;
};
