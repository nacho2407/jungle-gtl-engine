// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Light/SpotLightSceneProxy.h"

#include "Component/SpotLightComponent.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Render/Scene/Scene.h"

#include <cmath>

namespace
{
constexpr float kTwoPi    = 6.28318530f;
constexpr float kDegToRad = 0.01745329f;

void AddDebugCircle(FScene& Scene, const FVector& Center,
                    const FVector& AxisX, const FVector& AxisY,
                    float Radius, const FColor& Color, int Segments = 32)
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
                  float Length, float HalfAngleDegrees, const FColor& Color, int Segments = 32)
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

    AddDebugCircle(Scene, CircleCenter, AxisX, AxisY, Radius, Color, Segments);

    for (int Index = 0; Index < 4; ++Index)
    {
        const float   Angle = kTwoPi * static_cast<float>(Index) / 4.0f;
        const FVector Edge  = CircleCenter + AxisX * (cosf(Angle) * Radius) + AxisY * (sinf(Angle) * Radius);
        RenderDebugLine(Scene, Apex, Edge, Color);
    }
}
} // namespace

FSpotLightSceneProxy::FSpotLightSceneProxy(USpotLightComponent* InComponent)
    : FPointLightSceneProxy(InComponent)
{
    LightConstants.LightType = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateLightConstants()
{
    if (!Owner)
    {
        return;
    }

    FPointLightSceneProxy::UpdateLightConstants();

    USpotLightComponent* SpotLight = static_cast<USpotLightComponent*>(Owner);
    LightConstants.InnerConeAngle  = SpotLight->GetInnerConeAngle();
    LightConstants.OuterConeAngle  = SpotLight->GetOuterConeAngle();
    LightConstants.LightType       = static_cast<uint32>(ELightType::Spot);
}

void FSpotLightSceneProxy::UpdateTransform()
{
    FLightSceneProxy::UpdateTransform();
}

void FSpotLightSceneProxy::VisualizeLightsInEditor(FScene& Scene) const
{
    if (!Owner)
    {
        return;
    }

    USpotLightComponent* Component = static_cast<USpotLightComponent*>(Owner);
    const FVector        Apex      = Component->GetWorldLocation();
    const FVector        Direction = Component->GetForwardVector();
    const float          Length    = Component->GetAttenuationRadius();

    constexpr float ArrowLength = 2.0f;
    RenderDebugArrow(Scene, Apex, Direction, ArrowLength, FColor(255, 255, 0));
    AddDebugCone(Scene, Apex, Direction, Length, Component->GetInnerConeAngle(), FColor(255, 255, 0));
    AddDebugCone(Scene, Apex, Direction, Length, Component->GetOuterConeAngle(), FColor(255, 140, 0));
}
