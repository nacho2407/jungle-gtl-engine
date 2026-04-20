#include "DirectionalLightComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"
#include "Render/Scene/Proxies/Light/DirectionalLightSceneProxy.h"

namespace
{
void AddDirectionalLightArrow(FScene& Scene, const FVector& Start, const FVector& Direction)
{
    constexpr float ProjectileArrowScale = 0.25f;
    const FVector ScaledVelocity = Direction * ProjectileArrowScale;
    const float VelocityLength = ScaledVelocity.Length();
    if (VelocityLength <= FMath::Epsilon)
    {
        return;
    }

    const FVector End = Start + ScaledVelocity;
    const FColor ArrowColor(135, 206, 235);

    Scene.AddDebugLine(Start, End, ArrowColor);

    const float HeadLength = Clamp(VelocityLength * 0.2f, 0.2f, 1.5f);
    FVector ReferenceUp(0.0f, 0.0f, 1.0f);
    if (std::abs(Direction.Dot(ReferenceUp)) > 0.98f)
    {
        ReferenceUp = FVector(0.0f, 1.0f, 0.0f);
    }

    const FVector Side = Direction.Cross(ReferenceUp).Normalized();
    const FVector Back = Direction * HeadLength;
    const FVector SideOffset = Side * (HeadLength * 0.45f);

    Scene.AddDebugLine(End, End - Back + SideOffset, ArrowColor);
    Scene.AddDebugLine(End, End - Back - SideOffset, ArrowColor);
}
} // namespace

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
    // UE5 Directional Light Default Intensity
    Intensity = 10.0f;
}

void UDirectionalLightComponent::Serialize(FArchive& Ar)
{
    ULightComponent::Serialize(Ar);
    Ar << Direction;
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    ULightComponent::GetEditableProperties(OutProps);
}

void UDirectionalLightComponent::PostEditProperty(const char* PropertyName)
{
    ULightComponent::PostEditProperty(PropertyName);
}

FLightSceneProxy* UDirectionalLightComponent::CreateLightSceneProxy()
{
    return new FDirectionalLightSceneProxy(this);
}
