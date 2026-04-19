#include "DirectionalLightSceneProxy.h"
#include "Component/DirectionalLightComponent.h"
#include "Render/Proxy/FScene.h"
#include <cmath>

namespace
{
    constexpr float TwoPI = 6.28318530f;

    // TransGizmo와 동일한 비율: 원통 stem(80%) + 원뿔 head(20%)
    void AddArrow(FScene& Scene, const FVector& Start, const FVector& Dir, float Length, const FColor& Color, int Segs = 8)
    {
        const float StemLen    = Length * 0.8f;
        const float StemRadius = Length * 0.04f;
        const float HeadRadius = Length * 0.1f;
        const FVector Tip     = Start + Dir * Length;
        const FVector StemEnd = Start + Dir * StemLen;

        FVector WorldUp(0.f, 0.f, 1.f);
        if (fabsf(Dir.Dot(WorldUp)) > 0.98f) WorldUp = FVector(1.f, 0.f, 0.f);
        const FVector AxisX = Dir.Cross(WorldUp).Normalized();
        const FVector AxisY = Dir.Cross(AxisX);

        auto DrawCircle = [&](const FVector& Center, float Radius)
        {
            for (int i = 0; i < Segs; ++i)
            {
                float A0 = TwoPI * i / Segs;
                float A1 = TwoPI * (i + 1) / Segs;
                FVector P0 = Center + AxisX * (cosf(A0) * Radius) + AxisY * (sinf(A0) * Radius);
                FVector P1 = Center + AxisX * (cosf(A1) * Radius) + AxisY * (sinf(A1) * Radius);
                Scene.AddDebugLine(P0, P1, Color);
            }
        };

        // 원통 stem
        DrawCircle(Start, StemRadius);
        DrawCircle(StemEnd, StemRadius);
        for (int i = 0; i < 4; ++i)
        {
            float A = TwoPI * i / 4;
            FVector P = AxisX * (cosf(A) * StemRadius) + AxisY * (sinf(A) * StemRadius);
            Scene.AddDebugLine(Start + P, StemEnd + P, Color);
        }

        // 원뿔 head
        DrawCircle(StemEnd, HeadRadius);
        for (int i = 0; i < 4; ++i)
        {
            float A = TwoPI * i / 4;
            FVector P = AxisX * (cosf(A) * HeadRadius) + AxisY * (sinf(A) * HeadRadius);
            Scene.AddDebugLine(StemEnd + P, Tip, Color);
        }
    }
}

FDirectionalLightSceneProxy::FDirectionalLightSceneProxy(UDirectionalLightComponent* InComponent)
    : FLightSceneProxy(InComponent)
{
}

void FDirectionalLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
        return;

    FLightSceneProxy::UpdateLightConstants();
    LightConstants.LightType = static_cast<uint32>(ELightType::Directional);
}

void FDirectionalLightSceneProxy::VisualizeLightsInEditor(FScene& Scene) const
{
    if (!Owner) return;
    const FVector Origin = Owner->GetWorldLocation();
    const FVector Dir = Owner->GetForwardVector();
    constexpr float ArrowLen = 2.0f;
    const FColor Color(135, 206, 235);

    AddArrow(Scene, Origin, Dir, ArrowLen, Color);
}

