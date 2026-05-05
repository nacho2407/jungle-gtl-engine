#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "GameFramework/CameraManager.h"

IMPLEMENT_CLASS(APlayerController, AActor)

UCameraManager* APlayerController::GetPlayerCameraManager() const
{
	UWorld* World = GetWorld();
	return World ? World->GetCameraManager() : nullptr;
}

void APlayerController::SetViewTargetWithBlend(
	AActor* NewViewTarget,
	float BlendTime,
	EViewTargetBlendFunction BlendFunc,
	float BlendExp,
	bool bLockOutgoing)
{
	UCameraManager* CM = GetPlayerCameraManager();
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
