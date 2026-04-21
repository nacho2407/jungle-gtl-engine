#pragma once

#include "Render/Scene/Proxies/SceneProxy.h"

/*
    FSceneEffectProxy는 Primitive/Light와 별개로 Scene-wide effect를 표현하는 베이스 타입입니다.
    Height Fog처럼 기하 프리미티브가 아닌 효과 프록시가 이 계층을 사용합니다.
*/
class FSceneEffectProxy : public FSceneProxy
{
public:
    virtual ~FSceneEffectProxy() = default;
};
