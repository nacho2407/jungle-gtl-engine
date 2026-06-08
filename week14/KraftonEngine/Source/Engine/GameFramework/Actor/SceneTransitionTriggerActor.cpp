#include "GameFramework/Actor/SceneTransitionTriggerActor.h"

#include "Component/Shape/BoxComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Matrix.h"
#include "Runtime/Engine.h"

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
		}
		return;
	}

	// 카운트다운 중 박스를 벗어나면 리셋 — 단발 전이라면 중간 이탈 시 취소된다.
	if (!bInside)
	{
		bCountingDown = false;
		ElapsedSinceEnter = 0.0f;
		return;
	}

	ElapsedSinceEnter += DeltaTime;
	if (ElapsedSinceEnter >= TransitionDelay)
	{
		FireTransition();
	}
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

	UE_LOG("[SceneTransitionTrigger] %s -> %s (delay %.2fs)",
		GetFName().ToString().c_str(), TargetScene.c_str(), TransitionDelay);

	GEngine->RequestTransitionToScene(TargetScene);

	if (bTriggerOnce)
	{
		bConsumed = true;
	}
	bCountingDown = false;
	ElapsedSinceEnter = 0.0f;
}
