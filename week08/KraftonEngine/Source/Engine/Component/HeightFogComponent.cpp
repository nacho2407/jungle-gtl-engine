#include "HeightFogComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Render/Proxy/FScene.h"
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
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().ClearFog();
}

void UHeightFogComponent::OnTransformDirty()
{
	USceneComponent::OnTransformDirty();
	PushToScene();
}

void UHeightFogComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	FFogParams Params;
	Params.Density            = FogDensity;
	Params.HeightFalloff      = FogHeightFalloff;
	Params.StartDistance      = StartDistance;
	Params.CutoffDistance     = FogCutoffDistance;
	Params.MaxOpacity         = FogMaxOpacity;
	Params.FogBaseHeight      = GetWorldLocation().Z;
	Params.InscatteringColor  = FogInscatteringColor;

	World->GetScene().SetFog(Params);
}

void UHeightFogComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Fog Density",       EPropertyType::Float,  &FogDensity });
	OutProps.push_back({ "Height Falloff",    EPropertyType::Float,  &FogHeightFalloff });
	OutProps.push_back({ "Start Distance",    EPropertyType::Float,  &StartDistance });
	OutProps.push_back({ "Cutoff Distance",   EPropertyType::Float,  &FogCutoffDistance });
	OutProps.push_back({ "Max Opacity",       EPropertyType::Float,  &FogMaxOpacity });
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
