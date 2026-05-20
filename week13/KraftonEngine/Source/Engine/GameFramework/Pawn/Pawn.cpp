#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "Component/Camera/CameraComponent.h"
#include "Component/Input/InputComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Types/PropertyTypes.h"
#include "Serialization/Archive.h"


void APawn::BeginPlay()
{
	Super::BeginPlay();

	// Input 자동 부착 + 자식 setup. PostDuplicate 후에도 BeginPlay 가 다시 호출되므로
	// 중복 add 방지 위해 GetComponentByClass 로 기존 인스턴스 우선 회수.
	if (!InputComponent)
	{
		InputComponent = GetComponentByClass<UInputComponent>();
		if (!InputComponent)
		{
			InputComponent = AddComponent<UInputComponent>();
		}
	}
	SetupInputComponent();
}

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
	Ar << bUseControllerRotationPitch;
	Ar << bUseControllerRotationYaw;
	Ar << bUseControllerRotationRoll;
}

void APawn::ApplyControllerRotationToRoot()
{
	if (!bUseControllerRotationPitch && !bUseControllerRotationYaw && !bUseControllerRotationRoll) return;

	USceneComponent* Root = GetRootComponent();
	if (!Root) return;

	FRotator R = Root->GetRelativeRotation();
	if (bUseControllerRotationYaw)   R.Yaw   = ControlRotation.Yaw;
	if (bUseControllerRotationPitch) R.Pitch = ControlRotation.Pitch;
	if (bUseControllerRotationRoll)  R.Roll  = ControlRotation.Roll;
	Root->SetRelativeRotation(R);
}
