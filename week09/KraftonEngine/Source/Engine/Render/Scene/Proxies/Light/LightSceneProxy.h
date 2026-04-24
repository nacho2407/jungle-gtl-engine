// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resources/Buffers/LightBufferTypes.h"
#include "Render/Scene/Proxies/SceneProxy.h"

class ULightComponent;
class FScene;

// FLightSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FLightSceneProxy : public FSceneProxy
{
public:
    FLightSceneProxy(ULightComponent* InComponent);
    virtual ~FLightSceneProxy() = default;

    virtual void UpdateLightConstants();
    virtual void UpdateTransform();

    virtual void VisualizeLightsInEditor(FScene& Scene) const {}

    ULightComponent* Owner = nullptr;

    FLightConstants LightConstants = {};

    bool bVisible      = true;
    bool bAffectsWorld = true;
};
