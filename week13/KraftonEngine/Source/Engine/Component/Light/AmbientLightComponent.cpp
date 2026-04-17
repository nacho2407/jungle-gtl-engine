#include "AmbientLightComponent.h"
#include "Render/Types/GlobalLightParams.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponentBase)

void UAmbientLightComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;
	FGlobalAmbientLightParams Params;
	Params.Intensity = Intensity;
	Params.LightColor = LightColor;
	Params.bVisible = bVisible;

	World->GetScene().AddGlobalAmbientLight(this, Params);
}

void UAmbientLightComponent::DestroyFromScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().RemoveGlobalAmbientLight(this);
}
