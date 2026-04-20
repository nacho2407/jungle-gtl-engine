#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"
#include "Component/PointLightComponent.h"
#include "Render/Scene/Core/Scene.h"
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

	// 반각 180° = 전방향 구(sphere). cos(π) = -1이므로 SpotFactor가 항상 1이 됩니다.
	LightConstants.OuterConeAngle = 180.0f;
	LightConstants.InnerConeAngle = 180.0f;
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

void FPointLightSceneProxy::VisualizeLightsInEditor(FScene& Scene) const
{
    if (!Owner) return;
    UPointLightComponent* Comp = static_cast<UPointLightComponent*>(Owner);
    const FVector C = Comp->GetWorldLocation();
    const float   R = Comp->GetAttenuationRadius();
    const FColor  Color(255, 220, 100);

    AddDebugCircle(Scene, C, FVector(1,0,0), FVector(0,1,0), R, Color);
    AddDebugCircle(Scene, C, FVector(1,0,0), FVector(0,0,1), R, Color);
    AddDebugCircle(Scene, C, FVector(0,1,0), FVector(0,0,1), R, Color);

    // YZ 평면을 Z축 기준으로 22.5°씩 회전한 원 8개
    for (int i = 1; i <= 8; ++i)
    {
        float Angle = 3.14159265f * 0.125f * i; // 22.5° * i
        FVector AX(-sinf(Angle), cosf(Angle), 0.0f);
        AddDebugCircle(Scene, C, AX, FVector(0, 0, 1), R, Color);
    }

	// XY 평면을 따라 구를 8등분해서 구형 격자 그리기
    for (int i = 1; i <= 8; ++i)
    {
        float Angle = (- 3.14159265f / 2) + 3.14159265f * 0.125f * i; // 22.5° * i
        FVector NC = C + FVector(0, 0, R * sin(Angle));
        AddDebugCircle(Scene, NC, FVector(1, 0, 0), FVector(0, 1, 0), R * cos(Angle), Color);
    }
}
