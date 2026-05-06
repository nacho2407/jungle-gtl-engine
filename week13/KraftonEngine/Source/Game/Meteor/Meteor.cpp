#include "Game/Meteor/Meteor.h"
#include "Game/Pawn/CarPawn.h"
#include "Audio/AudioManager.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Mesh/ObjManager.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Core/CollisionTypes.h"
#include "Core/Log.h"

#include <algorithm>
#include <cstdlib>

IMPLEMENT_CLASS(AMeteor, AActor)

void AMeteor::InitDefaultComponents(const FString& StaticMeshFileName)
{
	CollisionSphere = AddComponent<USphereComponent>();
	SetRootComponent(CollisionSphere);
	CollisionSphere->SetSphereRadius(5.0f);
	// SetCollisionEnabled가 IsQueryCollisionEnabled 변화 시 PhysicsScene::RegisterComponent를
	// 즉시 호출하므로, SimulatePhysics/ObjectType/Response 등 모든 셋업을 끝낸 뒤에
	// 마지막으로 호출해야 PhysX가 올바른 값(Dynamic + Block 응답)으로 등록한다.
	CollisionSphere->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	// 운석끼리 충돌하면 HandleHit → 즉시 자기 destroy 로 지면 도달 전에 사라지는 문제.
	// 같은 WorldDynamic 채널끼리는 Ignore 로 두어 contact 자체가 안 일어나게 함.
	// (대신 같은 채널의 Capsule/Box/SphereActor 같은 generic 액터도 통과한다는 점 trade-off,
	// 실제 ground/player 타격은 WorldStatic / Pawn 채널이라 영향 없음.)
	CollisionSphere->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Ignore);
	CollisionSphere->SetSimulatePhysics(true);
	CollisionSphere->SetMass(750.0f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	Mesh = AddComponent<UStaticMeshComponent>();
	Mesh->AttachToComponent(CollisionSphere);
	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(StaticMeshFileName, Device))
			Mesh->SetStaticMesh(Asset);
		Mesh->SetRelativeScale(FVector(5.0f, 5.0f, 5.0f));
	}

	// 시각적인 텀블링 — root는 PhysX가 매 프레임 회전을 덮어쓰므로 Mesh를 회전시킨다.
	auto RandRange = [](float MinDeg, float MaxDeg)
	{
		const float T = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return MinDeg + T * (MaxDeg - MinDeg);
	};
	URotatingMovementComponent* Rotator = AddComponent<URotatingMovementComponent>();
	Rotator->SetUpdatedComponent(Mesh);
	// world-space + pivot=0이면 TickComponent가 early return해서 회전이 안 돈다.
	// 자기 자리 자전이므로 local-space로 켜야 AddLocalRotation 경로를 탄다.
	Rotator->SetRotationInLocalSpace(true);
	Rotator->SetRotationRate(FRotator(RandRange(-180.0f, 180.0f), RandRange(-180.0f, 180.0f), RandRange(-180.0f, 180.0f)));
}

void AMeteor::PostDuplicate()
{
	Super::PostDuplicate();
	ResolveCachedComponents();
}

void AMeteor::BeginPlay()
{
	if (!CollisionSphere)
	{
		InitDefaultComponents();
	}

	Super::BeginPlay();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentHit.AddRaw(this, &AMeteor::HandleHit);
	}
}

void AMeteor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// off-map 추락 방어 — lateral velocity 로 콜리전 없는 영역에 떨어진 운석이 Lifetime
	// 다 채울 때까지 살아있으면 spawn 한도(MAX_CONCURRENT) 를 잠식. Z 임계 이하면 즉시 회수.
	if (GetActorLocation().Z < UnderworldZ)
	{
		if (UWorld* W = GetWorld())
		{
			W->DestroyActor(this);
		}
		return;
	}

	// Ground impact fallback — PhysX wrapper 가 Dynamic vs Static (메테오 vs 지면) 의
	// OnComponentHit 콜백을 propagate 하지 않는 경우, velocity 가 거의 0 이 된 시점을
	// 지면 정착으로 간주해 PlayLandingFeedback 을 1회 발동. spawn 직후 launch velocity 가
	// 인가되기 전 frame 을 거르기 위해 ElapsedTime 가 일정 이상 흐른 뒤에만 검사.
	constexpr float kGroundCheckMinElapsed = 0.3f;
	constexpr float kGroundVelocityEpsilonSq = 1.0f;  // |v| < 1 m/s 면 정지 간주
	if (!bAlreadyExploded && CollisionSphere && ElapsedTime > kGroundCheckMinElapsed)
	{
		const FVector V = CollisionSphere->GetLinearVelocity();
		if (V.Dot(V) < kGroundVelocityEpsilonSq)
		{
			bAlreadyExploded = true;
			PlayLandingFeedback();
			ElapsedTime = Lifetime;  // 다음 분기에서 destroy
		}
	}

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= Lifetime)
	{
		if (UWorld* W = GetWorld())
		{
			W->DestroyActor(this);
		}
	}
}

void AMeteor::SetLaunchVelocity(const FVector& Vel)
{
	if (CollisionSphere)
	{
		CollisionSphere->SetLinearVelocity(Vel);
	}
}

void AMeteor::HandleHit(UPrimitiveComponent* /*HitComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, FVector /*Impulse*/, const FHitResult& /*Hit*/)
{
	// 운석끼리는 destroy 하지 않음. CollisionResponse 로 1차 차단했지만 혹시 contact 가
	// 들어와도 lifetime 만료 처리는 skip — 두 운석이 동시에 사라지는 사고 방지.
	if (OtherActor && OtherActor->IsA<AMeteor>())
	{
		return;
	}

	// PhysX 가 같은 임팩트에 contact 콜백을 여러 번 발행할 수 있어 첫 1회만 처리.
	if (bAlreadyExploded)
	{
		return;
	}
	bAlreadyExploded = true;

	// 차량에 데미지 적용 — 차량 외 액터(지면 등)와 충돌이면 데미지 없이 destroy만
	if (auto* Car = Cast<ACarPawn>(OtherActor))
	{
		Car->TakeMeteorDamage(DamagePerHit);
	}

	// 카메라 셰이크 + 임팩트 사운드 — 플레이어와의 거리 기반 falloff.
	PlayLandingFeedback();

	// PhysX onContact 콜백 안에서 즉시 DestroyActor 호출하면 PhysX scene 변경 시점이
	// fetchResults 도중과 겹쳐 위험. Lifetime을 만료시켜 다음 AMeteor::Tick에서 안전하게 destroy.
	ElapsedTime = Lifetime;
}

void AMeteor::PlayLandingFeedback()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 플레이어 차량 위치 — World 의 액터 중 첫 ACarPawn 을 사용 (lua 의 ObjRegistry.car
	// 패턴과 동일). 매 임팩트마다 순회하지만 빈도 낮아 cost 무시 가능.
	AActor* PlayerCar = nullptr;
	for (AActor* Actor : World->GetActors())
	{
		if (Actor && Actor->IsA<ACarPawn>())
		{
			PlayerCar = Actor;
			break;
		}
	}
	if (!PlayerCar) return;

	// 거리 0 → Falloff 1.0 (최대 강도), MaxAudibleDistance 이상 → 0 (무음/무흔들림).
	const FVector Diff = PlayerCar->GetActorLocation() - GetActorLocation();
	const float Distance = Diff.Length();
	const float Falloff = std::max(0.0f, std::min(1.0f - Distance / MaxAudibleDistance, 1.0f));
	if (Falloff <= 0.0f)
	{
		return;
	}

	const float ShakeScale = MinShakeScale + (MaxShakeScale - MinShakeScale) * Falloff;
	const float Volume     = MaxImpactVolume * Falloff;

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
		{
			CM->StartCameraShakeAsset(FString("Asset/Test.shake"), ShakeScale);
		}
	}

	// 사운드 키 "MeteorImpact" — AudioManager.LoadAudio 로 사전 등록되어 있어야 한다.
	// 미등록 키는 silent (PlayAudio 가 무시).
	FAudioManager::Get().PlayAudio(FString("MeteorBoom"), Volume);
}

void AMeteor::ResolveCachedComponents()
{
	CollisionSphere = Cast<USphereComponent>(GetRootComponent());
	Mesh = GetComponentByClass<UStaticMeshComponent>();
}