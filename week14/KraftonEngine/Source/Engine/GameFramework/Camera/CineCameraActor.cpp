#include "GameFramework/Camera/CineCameraActor.h"

#include "Component/Camera/CineCameraComponent.h"

void ACineCameraActor::InitDefaultComponents()
{
	CameraComponent = AddComponent<UCineCameraComponent>();
	SetRootComponent(CameraComponent);

	BillboardComponent = EnsureCameraBillboard();
}

void ACineCameraActor::PostDuplicate()
{
	Super::PostDuplicate();

	CameraComponent = GetComponentByClass<UCineCameraComponent>();
	BillboardComponent = EnsureCameraBillboard();
}

UCineCameraComponent* ACineCameraActor::GetCineCameraComponent() const
{
	return Cast<UCineCameraComponent>(GetCameraComponent());
}
