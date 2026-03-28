#include "EditorCameraPawn.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Component/CameraComponent.h"

IMPLEMENT_RTTI(AEditorCameraPawn, AActor)

void AEditorCameraPawn::PostConstruct()
{
	CameraCompenent = FObjectFactory::ConstructObject<UCameraComponent>(this);
	AddOwnedComponent(CameraCompenent);
}
