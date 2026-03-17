#include "Actor/AGridActor.h"

AGridActor::AGridActor()
{
	auto GridMesh = BuiltInMeshFactory::CreateGridMesh(20, 1.0f);

    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>();
    MeshComp->SetStaticMesh(GridMesh);
    MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    AddComponent(MeshComp);
    SetRootComponent(MeshComp);
}
