#include "Actor/ACube.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"
#include "Resource/BuiltInMeshFactory.h"

ACube::ACube()
    : APrimitiveActor()
{
    InitializeActor();
}

ACube::~ACube()
{
}

void ACube::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    if (!StaticMeshComponent)
    {
        return;
    }

    StaticMeshComponent->SetStaticMesh(InStaticMesh);
}

UStaticMesh* ACube::GetStaticMesh() const
{
    if (!StaticMeshComponent)
    {
        return nullptr;
    }

    return StaticMeshComponent->GetStaticMesh();
}

UStaticMeshComponent* ACube::GetStaticMeshComponent() const
{
    return StaticMeshComponent;
}

void ACube::InitializeActor()
{
    StaticMeshComponent = NewObject<UStaticMeshComponent>(this);
    SetPrimitiveComponent(StaticMeshComponent);
    SetStaticMesh(BuiltInMeshFactory::CreateCubeMesh(StaticMeshComponent));
}
