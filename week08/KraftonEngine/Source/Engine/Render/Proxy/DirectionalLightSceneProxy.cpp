#include "DirectionalLightSceneProxy.h"
#include "Component/DirectionalLightComponent.h"
#include "Render/Proxy/FScene.h"
#include <cmath>

namespace
{
    // 단일 화살표: Start → Dir 방향으로 Length만큼, 끝에 촉 추가
    void AddArrow(FScene& Scene, const FVector& Start, const FVector& Dir, float Length, const FColor& Color)
    {
        const FVector End = Start + Dir * Length;
        Scene.AddDebugLine(Start, End, Color);

        FVector WorldUp(0.f, 0.f, 1.f);
        if (fabsf(Dir.Dot(WorldUp)) > 0.98f) WorldUp = FVector(1.f, 0.f, 0.f);
        const FVector Side = Dir.Cross(WorldUp).Normalized();

        const float HeadLen = Length * 0.25f;
        const FVector HeadBase = End - Dir * HeadLen;
        Scene.AddDebugLine(End, HeadBase + Side * (HeadLen * 0.5f), Color);
        Scene.AddDebugLine(End, HeadBase - Side * (HeadLen * 0.5f), Color);
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

void FDirectionalLightSceneProxy::VisualizeLights(FScene& Scene) const
{
    if (!Owner) return;
    const FVector Origin = Owner->GetWorldLocation();
    const FVector Dir = Owner->GetForwardVector();
    constexpr float ArrowLen = 2.0f;
    const FColor Color(135, 206, 235);

    AddArrow(Scene, Origin, Dir, ArrowLen, Color);
}

