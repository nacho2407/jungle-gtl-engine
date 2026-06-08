#include "GameFramework/Actor/SceneTransitionTriggerActor.h"

#include "Component/Shape/BoxComponent.h"
#include "Core/Logging/Log.h"
#include "Core/Types/EngineTypes.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Matrix.h"
#include "Runtime/Engine.h"
#include "Runtime/GameEngine.h"

#include <cmath>

void ASceneTransitionTriggerActor::InitDefaultComponents()
{
	BoxComponent = AddComponent<UBoxComponent>();
	SetRootComponent(BoxComponent);
	BoxComponent->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	// PhysX 미사용 — 매 Tick 에서 직접 OBB 포함 검사. 다른 액터/Ragdoll 과의 물리적 충돌
	// 없이 통과시키고 가시 영역만 갖는다.
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetGenerateOverlapEvents(false);
	BoxComponent->SetSimulatePhysics(false);
	BoxComponent->SetEnableGravity(false);
}

void ASceneTransitionTriggerActor::PostDuplicate()
{
	Super::PostDuplicate();
	BoxComponent = Cast<UBoxComponent>(GetRootComponent());
	bCountingDown = false;
	bConsumed = false;
	bFadeOutStarted = false;
	ElapsedSinceEnter = 0.0f;
}

void ASceneTransitionTriggerActor::BeginPlay()
{
	// 코드/씬 양쪽 경로 모두 보정 — 직렬화 값이 들어왔어도 PhysX 가 끼지 않도록 collision off.
	if (!GetRootComponent())
	{
		InitDefaultComponents();
	}
	if (!BoxComponent)
	{
		BoxComponent = Cast<UBoxComponent>(GetRootComponent());
	}
	if (BoxComponent)
	{
		BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoxComponent->SetGenerateOverlapEvents(false);
		BoxComponent->SetSimulatePhysics(false);
	}

	Super::BeginPlay();
}

void ASceneTransitionTriggerActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bConsumed || !BoxComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPossessedPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	const bool bInside = IsPawnInsideBox(Pawn);

	if (!bCountingDown)
	{
		if (bInside)
		{
			bCountingDown = true;
			ElapsedSinceEnter = 0.0f;
			// fade-out 은 아직 시작 안 함 — TransitionDelay 가 지나야 시작.
		}
		return;
	}

	// Fade-out 이 시작되기 전까지는 box 이탈 시 카운트 취소.
	// fade-out 시작 후엔 이미 시각적으로 commit 됐으므로 그대로 진행.
	if (!bInside && !bFadeOutStarted)
	{
		bCountingDown = false;
		ElapsedSinceEnter = 0.0f;
		return;
	}

	ElapsedSinceEnter += DeltaTime;

	// 타임라인: [0, TransitionDelay) 대기 → [TransitionDelay, TransitionDelay+FadeOutDuration) fade-out
	//          → 그 끝에서 transition + 새 scene fade-in.
	if (!bFadeOutStarted && ElapsedSinceEnter >= TransitionDelay)
	{
		BeginFadeOut(PC->GetPlayerCameraManager());
	}

	const float TotalTimeToFire = TransitionDelay + FadeOutDuration;
	if (ElapsedSinceEnter >= TotalTimeToFire)
	{
		FireTransition();
	}
}

void ASceneTransitionTriggerActor::BeginFadeOut(APlayerCameraManager* CamMgr)
{
	// fade duration 이 0 이어도 이후 transition 까지의 '진행 중' 상태를 유지하기 위해
	// bFadeOutStarted 는 켠다 — 이 플래그 이후엔 박스 이탈 시 cancel 되지 않는다.
	bFadeOutStarted = true;
	if (!CamMgr || FadeOutDuration <= 0.0f) return;

	// alpha 0 → 1. bHoldWhenFinished=true 라 fade 가 transition 전 짧게 끝나도 검은 화면 유지.
	CamMgr->StartCameraFade(0.0f, 1.0f, FadeOutDuration, FLinearColor::Black(),
		/*bShouldFadeAudio=*/false, /*bHoldWhenFinished=*/true);
}

bool ASceneTransitionTriggerActor::IsPawnInsideBox(const APawn* Pawn) const
{
	if (!Pawn || !BoxComponent) return false;

	const FVector PawnLocation = Pawn->GetActorLocation();

	// Box의 world inverse matrix로 local space 좌표를 얻으면 회전/스케일이 자동 반영된다.
	const FMatrix& InvWorld = BoxComponent->GetWorldInverseMatrix();
	const FVector LocalPawn = InvWorld.TransformPositionWithW(PawnLocation);

	// SetBoxExtent로 등록된 unscaled extent — world matrix가 scale을 이미 적용해 주므로
	// local space 에서는 unscaled 값으로 비교한다.
	const FVector Extent = BoxComponent->GetUnscaledBoxExtent();

	return std::abs(LocalPawn.X) <= Extent.X
		&& std::abs(LocalPawn.Y) <= Extent.Y
		&& std::abs(LocalPawn.Z) <= Extent.Z;
}

void ASceneTransitionTriggerActor::FireTransition()
{
	if (bConsumed) return;

	if (TargetScene.empty())
	{
		UE_LOG("[SceneTransitionTrigger] %s — TargetScene 비어 있어 전이 생략",
			GetFName().ToString().c_str());
		if (bTriggerOnce) bConsumed = true;
		bCountingDown = false;
		return;
	}

	if (!GEngine)
	{
		return;
	}

	UE_LOG("[SceneTransitionTrigger] %s -> %s (delay %.2fs, fadeOut %.2fs, fadeIn %.2fs)",
		GetFName().ToString().c_str(), TargetScene.c_str(),
		TransitionDelay, FadeOutDuration, FadeInDuration);

	// 새 scene 의 PlayerCameraManager 가 만들어진 직후 적용될 fade-in 예약.
	// (현재 PC/CamMgr 는 이 호출 뒤 world destroy 와 함께 사라지므로 엔진 측 상태로 보관해야 한다.)
	if (FadeInDuration > 0.0f)
	{
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			GameEngine->SetPendingFadeIn(FadeInDuration, FLinearColor::Black());
		}
	}

	GEngine->RequestTransitionToScene(TargetScene);

	if (bTriggerOnce)
	{
		bConsumed = true;
	}
	bCountingDown = false;
	ElapsedSinceEnter = 0.0f;
}
