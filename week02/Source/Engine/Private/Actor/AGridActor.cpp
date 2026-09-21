#include "Actor/AGridActor.h"

AGridActor::AGridActor()
{
    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
    MeshComp->SetStaticMesh(BuiltInMeshFactory::CreateGridMesh(200, 1.0f, MeshComp));
    MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    AddComponent(MeshComp);
    SetRootComponent(MeshComp);
}
