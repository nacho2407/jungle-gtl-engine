#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/World.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"

IMPLEMENT_CLASS(APlayerController, AActor)

void APlayerController::BeginPlay()
{
	Super::BeginPlay();
	// E.2/3: PC 가 PlayerCameraManager 의 owner — UE 패턴.
	// PC 가 GameMode->StartMatch 에서 spawn 되는 시점엔 다른 액터들이 이미 BeginPlay 완료.
	// 그 사이에 BeginPlay 한 카메라 컴포넌트들은 PC 가 없어 등록 못 했으므로 여기서 catch up.
	if (UWorld* World = GetWorld())
	{
		PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
		if (PlayerCameraManager)
		{
			// 한 액터가 여러 카메라 컴포넌트(예: CarPawn 의 First/Third Person)를 가질 수
			// 있으므로 GetComponents 전체를 순회. GetComponentByClass 는 첫 번째만 반환하므로 부족.
			for (AActor* Actor : World->GetActors())
			{
				if (!Actor) continue;
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
					{
						PlayerCameraManager->RegisterCamera(Cam);
					}
				}
			}
			PlayerCameraManager->AutoPossessDefaultCamera();
		}
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
