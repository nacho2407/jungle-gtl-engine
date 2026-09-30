// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/SpotLightSceneProxy.h"

#include "Component/SpotLightComponent.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"

#include <cmath>

namespace
{
constexpr float kTwoPi                  = 6.28318530f;
constexpr float kDegToRad               = 0.01745329f;
constexpr int   kBaseConeCircleSegments = 64;
constexpr int   kBaseConeSideSegments   = 8;

int32 ComputeScaledSegments(int32 BaseSegments, float DebugScale, int32 MinSegments, int32 MaxSegments)
{
    const float EffectiveScale = (DebugScale > 0.0f) ? DebugScale : 1.0f;
    return static_cast<int32>(Clamp(std::round(static_cast<float>(BaseSegments) * EffectiveScale),
                                    static_cast<float>(MinSegments),
                                    static_cast<float>(MaxSegments)));
}

void AddDebugCircle(FScene& Scene, const FVector& Center,
                    const FVector& AxisX, const FVector& AxisY,
                    float Radius, const FColor& Color, int Segments = kBaseConeCircleSegments)
{
    for (int Index = 0; Index < Segments; ++Index)
    {
        const float   Angle0 = kTwoPi * static_cast<float>(Index) / static_cast<float>(Segments);
        const float   Angle1 = kTwoPi * static_cast<float>(Index + 1) / static_cast<float>(Segments);
        const FVector P0     = Center + AxisX * (cosf(Angle0) * Radius) + AxisY * (sinf(Angle0) * Radius);
        const FVector P1     = Center + AxisX * (cosf(Angle1) * Radius) + AxisY * (sinf(Angle1) * Radius);
        RenderDebugLine(Scene, P0, P1, Color);
    }
}

void AddDebugCone(FScene& Scene, const FVector& Apex, const FVector& Direction,
                  float Length, float HalfAngleDegrees, const FColor& Color,
                  int CircleSegments = kBaseConeCircleSegments, int SideSegments = kBaseConeSideSegments)
{
    if (HalfAngleDegrees <= 0.0f)
    {
        return;
    }

    const float Radius = Length * tanf(HalfAngleDegrees * kDegToRad);

    FVector WorldUp(0.0f, 0.0f, 1.0f);
    if (fabsf(Direction.Dot(WorldUp)) > 0.98f)
    {
        WorldUp = FVector(1.0f, 0.0f, 0.0f);
    }

    const FVector AxisX        = Direction.Cross(WorldUp).Normalized();
    const FVector AxisY        = Direction.Cross(AxisX).Normalized();
    const FVector CircleCenter = Apex + Direction * Length;

    AddDebugCircle(Scene, CircleCenter, AxisX, AxisY, Radius, Color, CircleSegments);

    for (int Index = 0; Index < SideSegments; ++Index)
    {
        const float   Angle = kTwoPi * static_cast<float>(Index) / static_cast<float>(SideSegments);
        const FVector Edge  = CircleCenter + AxisX * (cosf(Angle) * Radius) + AxisY * (sinf(Angle) * Radius);
        RenderDebugLine(Scene, Apex, Edge, Color);
    }
}
} // namespace

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
    : FLightProxy(InComponent)
{
    LightProxyInfo.LightType = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
    {
        return;
    }

    FLightProxy::UpdateLightConstants();

    USpotLightComponent* SpotLight   = static_cast<USpotLightComponent*>(Owner);
    LightProxyInfo.AttenuationRadius = SpotLight->GetAttenuationRadius();
    LightProxyInfo.InnerConeAngle    = SpotLight->GetInnerConeAngle();
    LightProxyInfo.OuterConeAngle    = SpotLight->GetOuterConeAngle();
    LightProxyInfo.LightType         = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateTransform()
{
    FLightProxy::UpdateTransform();
}

void FSpotLightSceneProxy::VisualizeLightsInEditor(FScene& Scene, float DebugScale) const
{
    if (!Owner)
    {
        return;
    }

    USpotLightComponent* Component      = static_cast<USpotLightComponent*>(Owner);
    const FVector        Apex           = Component->GetWorldLocation();
    const FVector        Direction      = Component->GetForwardVector();
    const float          Length         = Component->GetAttenuationRadius();
    const int32          CircleSegments = ComputeScaledSegments(kBaseConeCircleSegments, DebugScale, 12, 256);
    const int32          SideSegments   = ComputeScaledSegments(kBaseConeSideSegments, DebugScale, 4, 32);

    constexpr float ArrowLength = 2.0f;
    RenderDebugArrow(Scene, Apex, Direction, ArrowLength, FColor(255, 255, 0));
    AddDebugCone(Scene, Apex, Direction, Length, Component->GetInnerConeAngle(), FColor(255, 255, 0), CircleSegments, SideSegments);
    AddDebugCone(Scene, Apex, Direction, Length, Component->GetOuterConeAngle(), FColor(255, 140, 0), CircleSegments, SideSegments);
}
