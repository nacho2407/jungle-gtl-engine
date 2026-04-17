#include "DirectionalLightComponent.h"
#include "Render/Types/GlobalLightParams.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Engine/Serialization/Archive.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponentBase)

void UDirectionalLightComponent::PushToScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;
	FGlobalDirectionalLightParams Params;
	Params.Direction = Direction;
	Params.Intensity = Intensity;
	Params.LightColor = LightColor;

	World->GetScene().AddGlobalDirectionalLight(this, Params);
}



void UDirectionalLightComponent::DestroyFromScene()
{
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	World->GetScene().RemoveGlobalDirectionalLight(this);
}

void UDirectionalLightComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	ULightComponentBase::GetEditableProperties(OutProps);
	OutProps.push_back({ "Direction",EPropertyType::Vec3,&Direction });
}

void UDirectionalLightComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << Direction;
}
