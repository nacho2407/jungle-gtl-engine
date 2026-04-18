#include "PointLightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Render/Proxy/FScene.h"
#include <cmath>

namespace
{
    // 중심 Center, 법선 방향 AxisX×AxisY 평면의 원을 Segs개 선분으로 그립니다.
    void AddDebugCircle(FScene& Scene, const FVector& Center,
                        const FVector& AxisX, const FVector& AxisY,
                        float Radius, const FColor& Color, int Segs = 32)
    {
        constexpr float TwoPI = 6.28318530f;
        for (int i = 0; i < Segs; ++i)
        {
            float A0 = TwoPI * i / Segs;
            float A1 = TwoPI * (i + 1) / Segs;
            FVector P0 = Center + AxisX * (cosf(A0) * Radius) + AxisY * (sinf(A0) * Radius);
            FVector P1 = Center + AxisX * (cosf(A1) * Radius) + AxisY * (sinf(A1) * Radius);
            Scene.AddDebugLine(P0, P1, Color);
        }
    }
}

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
	LightConstants.LightType = static_cast<uint32>(ELightType::Point);
}

void FPointLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightSceneProxy::UpdateLightConstants();

    UPointLightComponent* PointLight = static_cast<UPointLightComponent*>(Owner);
    LightConstants.AttenuationRadius = PointLight->GetAttenuationRadius();
    LightConstants.LightType = static_cast<uint32>(ELightType::Point);
}

void FPointLightSceneProxy::UpdateTransform()
{
    if (!Owner)
        return;
    LightConstants.Position = Owner->GetWorldLocation();
}

void FPointLightSceneProxy::VisualizeLights(FScene& Scene) const
{
    if (!Owner) return;
    UPointLightComponent* Comp = static_cast<UPointLightComponent*>(Owner);
    const FVector C = Comp->GetWorldLocation();
    const float   R = Comp->GetAttenuationRadius();
    const FColor  Color(255, 220, 100);

    AddDebugCircle(Scene, C, FVector(1,0,0), FVector(0,1,0), R, Color);
    AddDebugCircle(Scene, C, FVector(1,0,0), FVector(0,0,1), R, Color);
    AddDebugCircle(Scene, C, FVector(0,1,0), FVector(0,0,1), R, Color);
}
