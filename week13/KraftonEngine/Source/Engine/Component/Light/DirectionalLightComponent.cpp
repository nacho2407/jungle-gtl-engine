#include "DirectionalLightComponent.h"
#include "Render/Types/GlobalLightParams.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Engine/Serialization/Archive.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent)


void UDirectionalLightComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	FVector WorldPos = GetWorldLocation();
	Scene.AddDebugLine(WorldPos, WorldPos + GetForwardVector() * 5.f, FColor::Red());
}

void UDirectionalLightComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	FGlobalDirectionalLightParams Params;
	Params.Direction = GetForwardVector();
	Params.Intensity = Intensity;
	Params.LightColor = LightColor;
	Params.bVisible = bVisible;

	World->GetScene().AddGlobalDirectionalLight(this, Params);
}

void UDirectionalLightComponent::DestroyFromScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().RemoveGlobalDirectionalLight(this);
}
