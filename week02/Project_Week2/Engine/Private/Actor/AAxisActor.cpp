#include "Actor/AAxisActor.h"

AAxisActor::AAxisActor()
{
	UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
	MeshComp->SetStaticMesh(BuiltInMeshFactory::CreateAxesMesh(MeshComp));
	MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

	AddComponent(MeshComp);
	SetRootComponent(MeshComp);
}
