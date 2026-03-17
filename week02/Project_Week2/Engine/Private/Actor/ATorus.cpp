#include "Actor/ATorus.h"

#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"
#include "Resource/BuiltInMeshFactory.h"

ATorus::ATorus()
    : APrimitiveActor()
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
    SetStaticMesh(BuiltInMeshFactory::CreateTorusMesh(64, 32, 1.2f, 0.35f));
}
