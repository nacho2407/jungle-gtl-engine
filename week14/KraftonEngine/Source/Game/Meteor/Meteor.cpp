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
	CollisionSphere->SetSphereRadius(0.5f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	CollisionSphere->SetSimulatePhysics(true);
	CollisionSphere->SetMass(50.0f);

	Mesh = AddComponent<UStaticMeshComponent>();
	Mesh->AttachToComponent(CollisionSphere);
	if (GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(StaticMeshFileName, Device))
			Mesh->SetStaticMesh(Asset);
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

	if (UWorld* W = GetWorld())
	{
		W->DestroyActor(this);
	}
}
