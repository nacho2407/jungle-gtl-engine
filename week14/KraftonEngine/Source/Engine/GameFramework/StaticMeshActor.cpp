#include "GameFramework/StaticMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/SphereComponent.h"
#include "Core/Log.h"
#include "Core/CollisionTypes.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor)

void AStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();

	// 충돌 테스트용 SphereComponent 설정
	USphereComponent* SphereComp = GetComponentByClass<USphereComponent>();
	if(SphereComp)
	{
		//SphereComp->SetCollisionResponseToChannel(ECollisionChannel::WorldDynamic, ECollisionResponse::Overlap);

		SphereComp->OnComponentBeginOverlap.AddLambda([](UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
		{
			UE_LOG("SphereComponent overlapped with Actor: %s", OtherActor->GetName().c_str());
		});

		SphereComp->OnComponentEndOverlap.AddLambda([](UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
		{
			UE_LOG("SphereComponent end overlap with Actor: %s", OtherActor->GetName().c_str());
		});

		SphereComp->OnComponentHit.AddLambda([](UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult)
		{
			UE_LOG("SphereComponent blocked with Actor: %s", OtherActor->GetName().c_str());
		});
	}
}

void AStaticMeshActor::InitDefaultComponents(const FString& UStaticMeshFileName)
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	SetRootComponent(StaticMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(UStaticMeshFileName, Device);

	StaticMeshComponent->SetStaticMesh(Asset);

	// UUID 텍스트 표시
	//TextRenderComponent = AddComponent<UTextRenderComponent>();
	//TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	//TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	//TextRenderComponent->AttachToComponent(StaticMeshComponent);
	//TextRenderComponent->SetFont(FName("Default"));

	// SubUV 파티클
	//SubUVComponent = AddComponent<USubUVComponent>();
	//SubUVComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	//SubUVComponent->SetParticle(FName("Explosion"));
	//SubUVComponent->AttachToComponent(StaticMeshComponent);
	//SubUVComponent->SetVisibility(true);
}