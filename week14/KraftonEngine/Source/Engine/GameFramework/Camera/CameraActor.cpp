#include "GameFramework/Camera/CameraActor.h"

#include "Component/Camera/CameraComponent.h"
#include "Component/Primitive/BillboardComponent.h"

void ACameraActor::InitDefaultComponents()
{
	CameraComponent = AddComponent<UCameraComponent>();
	SetRootComponent(CameraComponent);

	BillboardComponent = EnsureCameraBillboard();
}

void ACameraActor::PostDuplicate()
{
	Super::PostDuplicate();

	CameraComponent = GetComponentByClass<UCameraComponent>();
	BillboardComponent = EnsureCameraBillboard();
}

UCameraComponent* ACameraActor::GetCameraComponent() const
{
	return CameraComponent ? CameraComponent : GetComponentByClass<UCameraComponent>();
}

UBillboardComponent* ACameraActor::EnsureCameraBillboard()
{
	return CameraComponent ? CameraComponent->EnsureEditorBillboard() : nullptr;
}
