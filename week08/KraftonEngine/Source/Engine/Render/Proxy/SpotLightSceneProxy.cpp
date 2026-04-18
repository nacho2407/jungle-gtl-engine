#include "SpotLightSceneProxy.h"
#include "Component/SpotLightComponent.h"
#include "Render/Proxy/FScene.h"
#include <cmath>

namespace
{
    constexpr float TwoPI    = 6.28318530f;
    constexpr float DegToRad = 0.01745329f;

    void AddDebugCircle(FScene& Scene, const FVector& Center,
                        const FVector& AxisX, const FVector& AxisY,
                        float Radius, const FColor& Color, int Segs = 32)
    {
        for (int i = 0; i < Segs; ++i)
        {
            float A0 = TwoPI * i / Segs;
            float A1 = TwoPI * (i + 1) / Segs;
            FVector P0 = Center + AxisX * (cosf(A0) * Radius) + AxisY * (sinf(A0) * Radius);
            FVector P1 = Center + AxisX * (cosf(A1) * Radius) + AxisY * (sinf(A1) * Radius);
            Scene.AddDebugLine(P0, P1, Color);
        }
    }

    // 원뿔 와이어프레임: 꼭짓점 Apex, 축 Dir, 길이 Length, 반각(도) HalfAngleDeg
    void AddDebugCone(FScene& Scene, const FVector& Apex, const FVector& Dir,
                      float Length, float HalfAngleDeg, const FColor& Color, int Segs = 32)
    {
        if (HalfAngleDeg <= 0.f) return;

        const float Radius = Length * tanf(HalfAngleDeg * DegToRad);

        FVector WorldUp(0.f, 0.f, 1.f);
        if (fabsf(Dir.Dot(WorldUp)) > 0.98f) WorldUp = FVector(1.f, 0.f, 0.f);
        const FVector AxisX = Dir.Cross(WorldUp).Normalized();
        const FVector AxisY = Dir.Cross(AxisX);   // 직교 단위벡터

        const FVector CircleCenter = Apex + Dir * Length;

        // 원 끝면
        AddDebugCircle(Scene, CircleCenter, AxisX, AxisY, Radius, Color, Segs);

        // 꼭짓점 → 원 테두리 4방향 선
        for (int i = 0; i < 4; ++i)
        {
            float A = TwoPI * i / 4;
            FVector Edge = CircleCenter + AxisX * (cosf(A) * Radius) + AxisY * (sinf(A) * Radius);
            Scene.AddDebugLine(Apex, Edge, Color);
        }
    }
}

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
    : FPointLightSceneProxy(InComponent)
{
	LightConstants.LightType = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FPointLightSceneProxy::UpdateLightConstants();

    USpotLightComponent* SpotLight = static_cast<USpotLightComponent*>(Owner);
    LightConstants.InnerConeAngle = SpotLight->GetInnerConeAngle();
    LightConstants.OuterConeAngle = SpotLight->GetOuterConeAngle();
    LightConstants.LightType = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateTransform()
{
    FLightSceneProxy::UpdateTransform(); // Position + Direction (PointLight의 Position only를 건너뜀)
}

void FSpotLightSceneProxy::VisualizeLights(FScene& Scene) const
{
    if (!Owner) return;
    USpotLightComponent* Comp = static_cast<USpotLightComponent*>(Owner);
    const FVector Apex   = Comp->GetWorldLocation();
    const FVector Dir    = Comp->GetForwardVector();
    const float   Length = Comp->GetAttenuationRadius();

    AddDebugCone(Scene, Apex, Dir, Length, Comp->GetInnerConeAngle(), FColor(255, 255,   0));
    AddDebugCone(Scene, Apex, Dir, Length, Comp->GetOuterConeAngle(), FColor(255, 140,   0));
}