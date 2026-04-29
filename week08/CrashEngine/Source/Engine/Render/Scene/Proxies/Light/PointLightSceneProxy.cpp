// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/PointLightSceneProxy.h"

#include "Component/PointLightComponent.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"

#include <cmath>

namespace
{
constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 2.0f * kPi;

int32 ComputeScaledSegments(int32 BaseSegments, float DebugScale)
{
    const float EffectiveScale = (DebugScale > 0.0f) ? DebugScale : 1.0f;
    return static_cast<int32>(Clamp(std::round(static_cast<float>(BaseSegments) * EffectiveScale), 8.0f, 128.0f));
}

int32 ComputeScaledRingCount(float DebugScale)
{
    const float EffectiveScale = (DebugScale > 0.0f) ? DebugScale : 1.0f;
    return static_cast<int32>(Clamp(std::round(3.0f * EffectiveScale), 3.0f, 12.0f));
}

void AddCircle(FScene& Scene, const FVector& Center,
               const FVector& AxisX, const FVector& AxisY,
               float Radius, int32 Segments, const FColor& Color)
{
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const float Angle0 = kTwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
        const float Angle1 = kTwoPi * static_cast<float>(Index + 1) / static_cast<float>(Segments);

        const FVector P0 = Center + AxisX * (cosf(Angle0) * Radius) + AxisY * (sinf(Angle0) * Radius);
        const FVector P1 = Center + AxisX * (cosf(Angle1) * Radius) + AxisY * (sinf(Angle1) * Radius);
        RenderDebugLine(Scene, P0, P1, Color);
    }
}

void AddSphereRings(FScene& Scene, const FVector& Center, float Radius,
                    int32 Segments, int32 RingCount, const FColor& Color)
{
    const FVector AxisX(1.0f, 0.0f, 0.0f);
    const FVector AxisY(0.0f, 1.0f, 0.0f);
    const FVector AxisZ(0.0f, 0.0f, 1.0f);

    AddCircle(Scene, Center, AxisX, AxisY, Radius, Segments, Color);
    AddCircle(Scene, Center, AxisX, AxisZ, Radius, Segments, Color);
    AddCircle(Scene, Center, AxisY, AxisZ, Radius, Segments, Color);

    if (RingCount <= 3)
    {
        return;
    }

    for (int32 RingIndex = 1; RingIndex < RingCount - 1; ++RingIndex)
    {
        const float T = static_cast<float>(RingIndex) / static_cast<float>(RingCount - 1);
        const float PolarAngle = kPi * T;
        const float Offset = cosf(PolarAngle) * Radius;
        const float RingRadius = sinf(PolarAngle) * Radius;

        if (RingRadius <= 0.001f)
        {
            continue;
        }

        AddCircle(Scene, Center + AxisZ * Offset, AxisX, AxisY, RingRadius, Segments, Color);
        AddCircle(Scene, Center + AxisY * Offset, AxisX, AxisZ, RingRadius, Segments, Color);
        AddCircle(Scene, Center + AxisX * Offset, AxisY, AxisZ, RingRadius, Segments, Color);
    }
}
}

FPointLightSceneProxy::FPointLightSceneProxy(UPointLightComponent* InComponent)
    : FLightProxy(InComponent)
{
    LightProxyInfo.LightType      = static_cast<uint32>(ELightType::Point);
    LightProxyInfo.OuterConeAngle = 180.0f;
    LightProxyInfo.InnerConeAngle = 180.0f;
}

void FPointLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
    {
        return;
    }

    FLightProxy::UpdateLightConstants();

    UPointLightComponent* PointLight = static_cast<UPointLightComponent*>(Owner);
    LightProxyInfo.AttenuationRadius = PointLight->GetAttenuationRadius();
    LightProxyInfo.LightType         = static_cast<uint32>(ELightType::Point);
}

void FPointLightSceneProxy::UpdateTransform()
{
    if (!Owner)
    {
        return;
    }

    LightProxyInfo.Position = Owner->GetWorldLocation();
}

void FPointLightSceneProxy::VisualizeLightsInEditor(FScene& Scene, float DebugScale) const
{
    if (!Owner)
    {
        return;
    }

    UPointLightComponent* Component = static_cast<UPointLightComponent*>(Owner);
    const FVector         Center    = Component->GetWorldLocation();
    const float           Radius    = Component->GetAttenuationRadius();
    const FColor          Color(255, 220, 100);
    const int32           Segments  = ComputeScaledSegments(32, DebugScale);
    const int32           RingCount = ComputeScaledRingCount(DebugScale);

    AddSphereRings(Scene, Center, Radius, Segments, RingCount, Color);
}

