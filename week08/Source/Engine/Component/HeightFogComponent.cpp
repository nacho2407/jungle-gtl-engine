// 컴포넌트 영역의 세부 동작을 구현합니다.
#include "HeightFogComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Scene/Scene.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UHeightFogComponent, USceneComponent)

UHeightFogComponent::UHeightFogComponent()
{
    SetComponentTickEnabled(false);
}

void UHeightFogComponent::CreateRenderState()
{
    PushToScene();
}

void UHeightFogComponent::DestroyRenderState()
{
    if (!Owner)
        return;
    UWorld* World = Owner->GetWorld();
    if (!World)
        return;

    World->GetScene().RemoveFog(this);
}

void UHeightFogComponent::OnTransformDirty()
{
    USceneComponent::OnTransformDirty();
    PushToScene();
}

void UHeightFogComponent::PushToScene()
{
    if (!Owner)
        return;
    UWorld* World = Owner->GetWorld();
    if (!World)
        return;

    FFogSceneData FogData;
    FogData.Density           = FogDensity;
    FogData.HeightFalloff     = FogHeightFalloff;
    FogData.StartDistance     = StartDistance;
    FogData.CutoffDistance    = FogCutoffDistance;
    FogData.MaxOpacity        = FogMaxOpacity;
    FogData.FogBaseHeight     = GetWorldLocation().Z;
    FogData.InscatteringColor = FogInscatteringColor;

    World->GetScene().AddFog(this, FogData);
}

void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);

    //                                                                     Min      Max        Speed
    OutProps.push_back({ "Fog Density", EPropertyType::Float, &FogDensity, 0.0f, 0.05f, 0.001f });
    OutProps.push_back({ "Height Falloff", EPropertyType::Float, &FogHeightFalloff, 0.001f, 5.0f, 0.01f });
    OutProps.push_back({ "Start Distance", EPropertyType::Float, &StartDistance, 0.0f, 100000.0f, 1.0f });
    OutProps.push_back({ "Cutoff Distance", EPropertyType::Float, &FogCutoffDistance, 0.0f, 100000.0f, 1.0f });
    OutProps.push_back({ "Max Opacity", EPropertyType::Float, &FogMaxOpacity, 0.0f, 1.0f, 0.01f });
    OutProps.push_back({ "Inscattering Color", EPropertyType::Color4, &FogInscatteringColor });
}

void UHeightFogComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);
    PushToScene();
}

void UHeightFogComponent::Serialize(FArchive& Ar)
{
    USceneComponent::Serialize(Ar);

    Ar << FogDensity;
    Ar << FogHeightFalloff;
    Ar << StartDistance;
    Ar << FogCutoffDistance;
    Ar << FogMaxOpacity;
    Ar << FogInscatteringColor;
}
