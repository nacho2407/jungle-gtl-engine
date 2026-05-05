#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Component/CameraComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(APawn, AActor)

void APawn::PossessedBy(APlayerController* PC)
{
	Controller = PC;

	// 자기 첫 카메라 컴포넌트를 ActiveCamera로 — PIE 시작 시 시점이 Pawn 기준이 되도록.
	// 카메라 컴포넌트가 없으면 no-op (CameraManager의 기존 흐름이 다른 카메라를 선택).
	// E.2/2: PC->GetPlayerCameraManager() 경로 사용.
	if (UCameraComponent* MyCamera = GetComponentByClass<UCameraComponent>())
	{
		if (PC)
		{
			if (APlayerCameraManager* Mgr = PC->GetPlayerCameraManager())
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
