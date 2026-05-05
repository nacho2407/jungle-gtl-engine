#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Component/CameraComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(APawn, AActor)

void APawn::PossessedBy(APlayerController* PC)
{
	Controller = PC;

	// 자기 첫 카메라 컴포넌트를 ActiveCamera로 — PIE 시작 시 시점이 Pawn 기준이 되도록.
	// 카메라 컴포넌트가 없으면 no-op (CameraManager의 기존 흐름이 다른 카메라를 선택).
	if (UWorld* World = GetWorld())
	{
		if (UCameraComponent* MyCamera = GetComponentByClass<UCameraComponent>())
		{
			if (APlayerCameraManager* Mgr = World->GetCameraManager())
			{
				Mgr->SetActiveCamera(MyCamera);
				Mgr->Possess(MyCamera);
			}
		}
	}
}

void APawn::UnPossessed()
{
	Controller = nullptr;
}

void APawn::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << bAutoPossessPlayer;
}
