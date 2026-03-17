#include "Actor/ATorus.h"

#include "FUObjectFactory.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"

ATorus::ATorus()
    : APrimitiveActor()
{
    InitializeActor();
}

ATorus::ATorus(const FUObjectInitializer& ObjectInitializer)
    : APrimitiveActor(ObjectInitializer)
{
    InitializeActor();
}

ATorus::~ATorus()
{
}

void ATorus::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    if (!StaticMeshComponent)
    {
        return;
    }

    StaticMeshComponent->SetStaticMesh(InStaticMesh);
}

UStaticMesh* ATorus::GetStaticMesh() const
{
    if (!StaticMeshComponent)
    {
        return nullptr;
    }

    return StaticMeshComponent->GetStaticMesh();
}

UStaticMeshComponent* ATorus::GetStaticMeshComponent() const
{
    return StaticMeshComponent;
}

void ATorus::InitializeActor()
{
    StaticMeshComponent = NewObject<UStaticMeshComponent>("TorusStaticMeshComponent");
    SetPrimitiveComponent(StaticMeshComponent);
}
