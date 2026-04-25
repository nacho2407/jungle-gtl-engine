#pragma once

#include "Math/Vector.h"
#include "Render/Scene/Proxies/Effects/SceneEffectProxy.h"

class UHeightFogComponent;

// FFogSceneData는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FFogSceneData
{
    float    Density           = 0.02f;
    float    HeightFalloff     = 0.2f;
    float    StartDistance     = 0.0f;
    float    CutoffDistance    = 0.0f;
    float    MaxOpacity        = 1.0f;
    float    FogBaseHeight     = 0.0f;
    FVector4 InscatteringColor = FVector4(0.45f, 0.55f, 0.65f, 1.0f);
};

// FFogSceneProxy는 게임 객체를 렌더러가 사용할 제출 데이터로 변환합니다.
class FFogSceneProxy : public FSceneEffectProxy
{
public:
    FFogSceneProxy(const UHeightFogComponent* InOwner, const FFogSceneData& InData)
        : Owner(InOwner), FogData(InData)
    {
    }

    void UpdateData(const FFogSceneData& InData)
    {
        FogData    = InData;
        DirtyFlags = ESceneProxyDirtyFlag::All;
    }

    const UHeightFogComponent* GetOwner() const { return Owner; }
    const FFogSceneData&       GetFogData() const { return FogData; }

private:
    const UHeightFogComponent* Owner = nullptr;
    FFogSceneData              FogData;
};
