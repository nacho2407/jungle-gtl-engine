#include "Actor/AGridActor.h"

AGridActor::AGridActor()
{
    // (*) 이거 InitializeResource에 있는데..확인 해봐야함
	auto GridMesh = BuiltInMeshFactory::CreateGridMesh(200, 1.0f);

    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
    MeshComp->SetStaticMesh(GridMesh);
    MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));

    AddComponent(MeshComp);
    SetRootComponent(MeshComp);
}
