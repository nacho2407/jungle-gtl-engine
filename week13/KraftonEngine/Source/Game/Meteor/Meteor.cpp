#include "Game/Meteor/Meteor.h"
#include "Game/Pawn/CarPawn.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/ObjManager.h"
#include "GameFramework/World.h"
#include "Core/CollisionTypes.h"
#include "Core/Log.h"

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
		Car->TakeDamage(DamagePerHit);
	}

	// PhysX onContact 콜백 안에서 즉시 DestroyActor 호출하면 PhysX scene 변경 시점이
	// fetchResults 도중과 겹쳐 위험. Lifetime을 만료시켜 다음 AMeteor::Tick에서 안전하게 destroy.
	ElapsedTime = Lifetime;
}
