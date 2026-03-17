#include "Actor/ASphere.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"

ASphere::ASphere()
    : APrimitiveActor()
{
    InitializeActor();
}

ASphere::~ASphere()
{
}

void ASphere::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    if (!StaticMeshComponent)
    {
        return;
    }

    StaticMeshComponent->SetStaticMesh(InStaticMesh);
}

UStaticMesh* ASphere::GetStaticMesh() const
{
    if (!StaticMeshComponent)
    {
        return nullptr;
    }

    return StaticMeshComponent->GetStaticMesh();
}

UStaticMeshComponent* ASphere::GetStaticMeshComponent() const
{
    return StaticMeshComponent;
}

void ASphere::InitializeActor()
{
    StaticMeshComponent = new UStaticMeshComponent();
    SetPrimitiveComponent(StaticMeshComponent);
}