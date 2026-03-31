#include "Engine/StaticMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor)

void AStaticMeshActor::InitDefaultComponents(const FString& UStaticMeshFileName)
{
	StaticMeshComponent = AddComponent<UStaticMeshComp>();
	SetRootComponent(StaticMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(UStaticMeshFileName, Device);

	StaticMeshComponent->SetStaticMesh(Asset);

	// UUID 텍스트 표시
	TextRenderComponent = AddComponent<UTextRenderComponent>();
	TextRenderComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 1.3f));
	TextRenderComponent->SetText("UUID : " + TextRenderComponent->GetOwnerUUIDToString());
	TextRenderComponent->AttachToComponent(StaticMeshComponent);
	TextRenderComponent->SetFont(FName("Default"));

	// SubUV 파티클
	SubUVComponent = AddComponent<USubUVComponent>();
	SubUVComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f));
	SubUVComponent->SetParticle(FName("Explosion"));
	SubUVComponent->AttachToComponent(StaticMeshComponent);
	//SubUVComponent->SetVisibility(true);
}