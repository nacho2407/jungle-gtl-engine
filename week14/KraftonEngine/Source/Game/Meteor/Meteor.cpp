#include "Game/Meteor/Meteor.h"
#include "Game/Pawn/CarPawn.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/Movement/RotatingMovementComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/ObjManager.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Core/CollisionTypes.h"
#include "Core/Log.h"

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
	CollisionSphere = Cast<USphereComponent>(GetRootComponent());
	Mesh = GetComponentByClass<UStaticMeshComponent>();
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

	ElapsedTime += DeltaTime;
	if (ElapsedTime >= Lifetime)
	{
		if (UWorld* W = GetWorld())
		{
			W->DestroyActor(this);
		}
	}
}

void AMeteor::HandleHit(UPrimitiveComponent* /*HitComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, FVector /*Impulse*/, const FHitResult& /*Hit*/)
{
	// 차량에 데미지 적용 — 차량 외 액터(지면 등)와 충돌이면 데미지 없이 destroy만
	if (auto* Car = Cast<ACarPawn>(OtherActor))
	{
		Car->TakeMeteorDamage(DamagePerHit);
	}

	// PhysX onContact 콜백 안에서 즉시 DestroyActor 호출하면 PhysX scene 변경 시점이
	// fetchResults 도중과 겹쳐 위험. Lifetime을 만료시켜 다음 AMeteor::Tick에서 안전하게 destroy.
	ElapsedTime = Lifetime;
}
