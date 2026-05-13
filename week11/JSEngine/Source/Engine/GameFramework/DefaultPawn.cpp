#include "GameFramework/DefaultPawn.h"

#include "Component/CameraComponent.h"

DEFINE_CLASS(ADefaultPawn, APawn)
REGISTER_FACTORY(ADefaultPawn)

void ADefaultPawn::InitDefaultComponents()
{
    CameraComp = AddComponent<UCameraComponent>();
    SetRootComponent(CameraComp);
    AddTag("DefaultPawn");
}
