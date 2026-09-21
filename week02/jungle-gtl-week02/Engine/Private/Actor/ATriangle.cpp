#include "Actor/ATriangle.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"
#include "Resource/BuiltInMeshFactory.h"

ATriangle::ATriangle()
    : APrimitiveActor()
{
    InitializeActor();
}

ATriangle::~ATriangle()
{
}

void ATriangle::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    if (!StaticMeshComponent)
    {
        return;
    }

    StaticMeshComponent->SetStaticMesh(InStaticMesh);
}

UStaticMesh* ATriangle::GetStaticMesh() const
{
    if (!StaticMeshComponent)
    {
        return nullptr;
    }

    return StaticMeshComponent->GetStaticMesh();
}

UStaticMeshComponent* ATriangle::GetStaticMeshComponent() const
{
    return StaticMeshComponent;
}

void ATriangle::InitializeActor()
{
    StaticMeshComponent = NewObject<UStaticMeshComponent>(this);
    SetPrimitiveComponent(StaticMeshComponent);
    SetStaticMesh(BuiltInMeshFactory::CreateTriangleMesh(StaticMeshComponent));
}
