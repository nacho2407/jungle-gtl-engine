#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerCameraManager.h"

IMPLEMENT_CLASS(APlayerController, AActor)

void APlayerController::BeginPlay()
{
	Super::BeginPlay();
	// E.2/1: World 의 CameraManager 를 reference 로 캐싱. E.3 에서 직접 SpawnActor 로 전환 예정.
	if (UWorld* World = GetWorld())
	{
		PlayerCameraManager = World->GetCameraManager();
	}
}

void APlayerController::SetViewTargetWithBlend(
	AActor* NewViewTarget,
	float BlendTime,
	EViewTargetBlendFunction BlendFunc,
	float BlendExp,
	bool bLockOutgoing)
{
	APlayerCameraManager* CM = GetPlayerCameraManager();
	if (!CM) return;

	FViewTargetTransitionParams Params;
	Params.BlendTime = BlendTime;
	Params.BlendFunction = BlendFunc;
	Params.BlendExp = BlendExp;
	Params.bLockOutgoing = bLockOutgoing;

	CM->SetViewTarget(NewViewTarget, Params);
}

void APlayerController::Possess(APawn* Pawn)
{
	if (!Pawn || PossessedPawn == Pawn) return;

	if (PossessedPawn)
	{
		UnPossess();
	}

	PossessedPawn = Pawn;
	Pawn->PossessedBy(this);
}

void APlayerController::UnPossess()
{
	if (!PossessedPawn) return;

	APawn* OldPawn = PossessedPawn;
	PossessedPawn = nullptr;
	OldPawn->UnPossessed();
}
