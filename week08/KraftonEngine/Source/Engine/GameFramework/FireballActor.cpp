#include "FireballActor.h"

#include "Component/DecalComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Materials/MaterialManager.h"
#include "Runtime/Engine.h"

IMPLEMENT_CLASS(AFireballActor, AActor);

AFireballActor::AFireballActor()
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void AFireballActor::InitDefaultComponents()
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(FireballMeshName, Device);
	StaticMeshComponent->SetStaticMesh(Asset);
	SetRootComponent(StaticMeshComponent);

	auto AdditiveDecalMaterial = FMaterialManager::Get().GetOrCreateMaterial(LightAreaMaterialPath);
	for (int i=0; i<3; i++)
	{
		DecalComponents[i] = AddComponent<UDecalComponent>();
		DecalComponents[i]->SetMaterial(AdditiveDecalMaterial);
		DecalComponents[i]->SetRelativeScale({10.0f, 10.0f, 10.0f});
		DecalComponents[i]->AttachToComponent(StaticMeshComponent);
	}
	DecalComponents[1]->SetRelativeRotation(FRotator(90, 0, 0));
	DecalComponents[2]->SetRelativeRotation(FRotator(0, 90, 0));
}
