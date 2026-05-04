#include "Game/Pawn/WalkingPersonActor.h"

#include "Component/BoxComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/LuaScriptComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/TriggerVolumeBase.h"
#include "GameFramework/World.h"
#include "Game/GameState/GameStateCarGame.h"
#include "Mesh/ObjManager.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Core/CollisionTypes.h"
#include "Core/Log.h"

IMPLEMENT_CLASS(AWalkingPersonActor, AActor)

void AWalkingPersonActor::InitDefaultComponents(const FString& StaticMeshFileName, const FString& LuaScriptFile)
{
	// Map.Scene 에 직접 셋업했던 ABoxActor + Mesh + LuaScript 패턴을 한 클래스로 묶음.
	// 1) Root = Person 충돌 박스 (Pawn 채널, simulate physics)
	CollisionBox = AddComponent<UBoxComponent>();
	SetRootComponent(CollisionBox);
	CollisionBox->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECollisionChannel::Pawn);
	// 다른 사람 / 차량 등과 모두 overlap (Map.Scene 직렬화 값과 동일 — 응답 2 = Overlap).
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	CollisionBox->SetSimulatePhysics(true);
	CollisionBox->SetMass(50.0f);
	CollisionBox->SetCenterOfMass(FVector(0.0f, 0.0f, -0.2f));

	// 2) Person 메시 (Box 자식 — 시각만)
	Mesh = AddComponent<UStaticMeshComponent>();
	Mesh->AttachToComponent(CollisionBox);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.677f));
	Mesh->SetRelativeRotation(FRotator(89.9f, 0.0f, 0.0f));
	Mesh->SetRelativeScale(FVector(1.5f, 1.5f, 1.5f));
	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(StaticMeshFileName, Device))
			Mesh->SetStaticMesh(Asset);
	}

	// 3) Lua 스크립트 — 회전/이동 / OnHit 정지 처리
	LuaScript = AddComponent<ULuaScriptComponent>();
	if (!LuaScriptFile.empty())
	{
		LuaScript->SetScriptFile(LuaScriptFile);
	}
}

void AWalkingPersonActor::BeginPlay()
{
	Super::BeginPlay();
	ResolveCachedComponents();

	// 시작 시점 위치/회전 캐시 — Phase 전환 시마다 여기로 되돌아간다.
	InitialLocation = GetActorLocation();
	InitialRotation = GetActorRotation();
	bInitialCached  = true;

	// Trigger 동적 spawn — 위치 sync 는 매 Tick 에서 수동.
	SpawnTrigger();

	// GameState 는 GameMode::StartMatch 에서 spawn 되는데, 그 호출이 World->BeginPlay
	// 끝부분에서 일어나므로 actor BeginPlay 시점엔 아직 nullptr 일 수 있음. Tick 에서
	// 처음 valid 한 GS 를 만나면 한 번 바인딩.
	EnsurePhaseListenerBound();
}

void AWalkingPersonActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bPhaseListenerBound)
	{
		EnsurePhaseListenerBound();
	}

	// 사람이 lua 의 SetLinearVelocity 로 걷는 동안 트리거가 따라오도록 매 frame 위치를
	// 사람의 현재 위치로 set. attachment 안 쓰는 이유는 cross-actor attach 가 PhysX
	// 측 sub-actor static body 의 setGlobalPose 와 미묘하게 안 맞아서, 단순 SetActorLocation
	// 으로 매 프레임 갱신하는 게 검증하기 쉬움.
	if (Trigger)
	{
		Trigger->SetActorLocation(GetActorLocation());
	}
}

void AWalkingPersonActor::EndPlay()
{
	Super::EndPlay();

	// 동적 spawn 한 trigger 도 같이 정리 — 안 그러면 person 죽어도 trigger 가 stale 위치에 남음.
	if (Trigger)
	{
		if (UWorld* W = GetWorld())
		{
			W->DestroyActor(Trigger);
		}
		Trigger = nullptr;
	}

	// PhaseChanged delegate 바인딩 해제 — GameState 가 더 길게 살아있을 경우 다음 broadcast 가
	// freed this 의 HandlePhaseChanged 호출하는 사고 방지.
	if (bPhaseListenerBound)
	{
		if (UWorld* W = GetWorld())
		{
			if (auto* GS = Cast<AGameStateCarGame>(W->GetGameState()))
			{
				GS->OnPhaseChanged.Remove(PhaseChangedHandle);
			}
		}
		bPhaseListenerBound = false;
	}
}

void AWalkingPersonActor::PostDuplicate()
{
	Super::PostDuplicate();
	ResolveCachedComponents();
}

void AWalkingPersonActor::ResolveCachedComponents()
{
	CollisionBox = Cast<UBoxComponent>(GetRootComponent());
	Mesh         = GetComponentByClass<UStaticMeshComponent>();
	LuaScript    = GetComponentByClass<ULuaScriptComponent>();
}

void AWalkingPersonActor::SpawnTrigger()
{
	UWorld* W = GetWorld();
	if (!W || !CollisionBox) return;

	// SpawnActor 안에서 ATriggerVolumeBase::BeginPlay 가 default InitDefaultComponents 를
	// 자동 호출 (extent 1,1,1) + 컴포넌트 BeginPlay 통해 PhysX 등록 + overlap 델리게이트 바인딩.
	Trigger = W->SpawnActor<ATriggerVolumeBase>();
	if (!Trigger) return;

	Trigger->SetFName(FName("EscapePolice"));
	Trigger->SetTriggerTag(FName("EscapePolice"));

	// Default 1m³ → 사람 주변 5×5×3 으로 확장. SetBoxExtent 가 NotifyPhysicsBodyDirty 까지
	// 호출해 PhysX shape 도 새 사이즈로 rebuild.
	if (UBoxComponent* TB = Trigger->GetTriggerBox())
	{
		TB->SetBoxExtent(FVector(TriggerExtentX, TriggerExtentY, TriggerExtentZ));
	}

	// 시작 위치를 사람과 동일하게 — 이후 Tick 에서 매 frame SetActorLocation 으로 갱신.
	Trigger->SetActorLocation(GetActorLocation());

	UE_LOG("[WalkingPerson] Spawned EscapePolice trigger");
}

void AWalkingPersonActor::EnsurePhaseListenerBound()
{
	if (bPhaseListenerBound) return;

	UWorld* W = GetWorld();
	if (!W) return;
	auto* GS = Cast<AGameStateCarGame>(W->GetGameState());
	if (!GS) return;

	PhaseChangedHandle = GS->OnPhaseChanged.AddRaw(this, &AWalkingPersonActor::HandlePhaseChanged);
	bPhaseListenerBound = true;
}

void AWalkingPersonActor::HandlePhaseChanged(ECarGamePhase NewPhase)
{
	// Phase 가 바뀔 때마다 (None → CarWash, CarWash → Result, Result → None 등) 시작 위치/
	// 회전으로 복원. 사람이 어디로 걸어갔든, 차에 부딪혀 누워있든 다음 페이즈 시작에는
	// 처음 자리에서 다시 시작.
	if (NewPhase == ECarGamePhase::None)
	{
		ResetToInitialTransform();
	}
}

void AWalkingPersonActor::ResetToInitialTransform()
{
	if (!bInitialCached) return;

	SetActorLocation(InitialLocation);
	SetActorRotation(InitialRotation);

	// PhysX body 의 잔여 momentum 도 0 으로 — 초기 위치에서 다시 정지 상태로 시작.
	if (CollisionBox)
	{
		CollisionBox->SetLinearVelocity(FVector(0.0f, 0.0f, 0.0f));
		CollisionBox->SetAngularVelocity(FVector(0.0f, 0.0f, 0.0f));
	}

	// 트리거도 같이 reset — 다음 frame Tick 이 어차피 sync 하지만, 한 frame 의 시각 차이도
	// 없도록 즉시 갱신.
	if (Trigger)
	{
		Trigger->SetActorLocation(InitialLocation);
	}

	// Lua 측 isMoving flag 도 다시 true 로 — 차에 부딪혀 stop 된 사람도 다음 phase 에서 walking 재개.
	// (lua 가 정의한 ResetWalkingState 함수가 없으면 조용히 no-op.)
	if (LuaScript)
	{
		LuaScript->CallFunction("ResetWalkingState");
	}
}
