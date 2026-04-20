#include "DecalComponent.h"

#include "Materials/MaterialManager.h"
#include "Collision/OBB.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Render/DebugDraw/DrawDebugHelpers.h"
#include "Render/Scene/Proxies/Primitive/DecalSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Mesh/ObjManager.h"
#include "Engine/Runtime/Engine.h"
#include "Texture/Texture2D.h"
#include "Materials/Material.h"
#include <algorithm>

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    if (TickType == ELevelTick::LEVELTICK_All)
    {
        HandleFade(DeltaTime);
    }

    UpdateReceivers();
    DrawDebugBox();
}

FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{
    return new FDecalSceneProxy(this);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Material", EPropertyType::MaterialSlot, &MaterialSlot });
    OutProps.push_back({ "Color", EPropertyType::Vec4, &Color });
    OutProps.push_back({ "FadeInDelay", EPropertyType::Float, &FadeInDelay });
    OutProps.push_back({ "FadeInDuration", EPropertyType::Float, &FadeInDuration });
    OutProps.push_back({ "FadeOutDelay", EPropertyType::Float, &FadeOutDelay });
    OutProps.push_back({ "FadeOutDuration", EPropertyType::Float, &FadeOutDuration });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);

    if (strcmp(PropertyName, "Material") == 0)
    {
        if (MaterialSlot.Path == "None" || MaterialSlot.Path.empty())
        {
            SetMaterial(0, nullptr);
        }
        else
        {
            UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot.Path);
            if (LoadedMat)
            {
                SetMaterial(0, LoadedMat);
            }
        }
        MarkRenderStateDirty();
    }
    if (strcmp(PropertyName, "Color") == 0)
    {
        MarkProxyDirty(EDirtyFlag::Material);
    }
}

void UDecalComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);
    Ar << MaterialSlot.Path;
    Ar << Color;
    Ar << FadeInDelay;
    Ar << FadeInDuration;
    Ar << FadeOutDelay;
    Ar << FadeOutDuration;
}

void UDecalComponent::PostDuplicate()
{
    UPrimitiveComponent::PostDuplicate();

    if (!MaterialSlot.Path.empty() && MaterialSlot.Path != "None")
    {
        UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot.Path);
        if (LoadedMat)
        {
            SetMaterial(0, LoadedMat);
        }
    }
    MarkProxyDirty(EDirtyFlag::Material);
}

FVector4 UDecalComponent::GetColor() const
{
    FVector4 OutColor = Color;
    OutColor.A *= Clamp(FadeOpacity, 0, 1);
    return OutColor;
}

void UDecalComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
    Material = InMaterial;
    if (Material)
    {
        MaterialSlot.Path = Material->GetAssetPathFileName();
    }
    else
    {
        MaterialSlot.Path = "None";
    }
    MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::UpdateDecalVolumeFromTransform()
{
    ConvexVolume.UpdateAsOBB(GetWorldMatrix());
}

void UDecalComponent::OnTransformDirty()
{
    UPrimitiveComponent::OnTransformDirty();
    UpdateReceivers();
}

void UDecalComponent::HandleFade(float DeltaTime)
{
    FadeTimer += DeltaTime;

    float Alpha = 1.0f;

    if (FadeInDuration > 0.0f)
    {
        const float InStart = FadeInDelay;
        const float InEnd = FadeInDelay + FadeInDuration;
        if (FadeTimer < InStart)
        {
            Alpha = 0.0f;
        }
        else if (FadeTimer < InEnd)
        {
            Alpha = (FadeTimer - InStart) / FadeInDuration;
        }
    }

    if (FadeOutDuration > 0.0f)
    {
        const float OutStart = FadeOutDelay;
        const float OutEnd = FadeOutDelay + FadeOutDuration;
        if (FadeTimer > OutEnd)
        {
            Alpha = 0.0f;
        }
        else if (FadeTimer > OutStart)
        {
            Alpha = std::min(Alpha, 1.0f - (FadeTimer - OutStart) / FadeOutDuration);
        }
    }

    FadeOpacity = Alpha;
    MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::UpdateReceivers()
{
    SCOPE_STAT_CAT("UpdateDecalReceivers", "6_Decal");

    UpdateDecalVolumeFromTransform();

    UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
    if (!World)
    {
        return;
    }

    TArray<UPrimitiveComponent*> OverlappingPrimitives;
    World->GetPartition().QueryFrustumAllPrimitive(ConvexVolume, OverlappingPrimitives);

    Receivers.clear();

    FOBB DecalOBB;
    DecalOBB.UpdateAsOBB(GetWorldMatrix());

    for (UPrimitiveComponent* PrimitiveComp : OverlappingPrimitives)
    {
        if (PrimitiveComp == this || PrimitiveComp->GetOwner() == GetOwner())
        {
            continue;
        }

        UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(PrimitiveComp);
        if (!StaticMeshComp || !StaticMeshComp->GetStaticMesh())
        {
            continue;
        }

        const FBoundingBox ReceiverBounds = StaticMeshComp->GetWorldBoundingBox();
        if (!ReceiverBounds.IsValid())
        {
            continue;
        }

        if (!DecalOBB.IntersectOBBAABB(ReceiverBounds))
        {
            continue;
        }

        Receivers.push_back(StaticMeshComp);
    }

    MarkProxyDirty(EDirtyFlag::Mesh);
}

void UDecalComponent::DrawDebugBox()
{
    const FMatrix& WorldMatrix = GetWorldMatrix();
    FVector P[8] = {
        FVector(-0.5f, -0.5f, -0.5f) * WorldMatrix,
        FVector(0.5f, -0.5f, -0.5f) * WorldMatrix,
        FVector(0.5f, 0.5f, -0.5f) * WorldMatrix,
        FVector(-0.5f, 0.5f, -0.5f) * WorldMatrix,
        FVector(-0.5f, -0.5f, 0.5f) * WorldMatrix,
        FVector(0.5f, -0.5f, 0.5f) * WorldMatrix,
        FVector(0.5f, 0.5f, 0.5f) * WorldMatrix,
        FVector(-0.5f, 0.5f, 0.5f) * WorldMatrix
    };

    UWorld* World = GetOwner()->GetWorld();

    DrawDebugLine(World, P[0], P[1], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[1], P[2], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[2], P[3], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[3], P[0], FColor::Green(), 0.0f);

    DrawDebugLine(World, P[4], P[5], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[5], P[6], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[6], P[7], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[7], P[4], FColor::Green(), 0.0f);

    DrawDebugLine(World, P[0], P[4], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[1], P[5], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[2], P[6], FColor::Green(), 0.0f);
    DrawDebugLine(World, P[3], P[7], FColor::Green(), 0.0f);
}
