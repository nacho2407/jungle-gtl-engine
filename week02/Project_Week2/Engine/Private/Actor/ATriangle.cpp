#include "Actor/ATriangle.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"

ATriangle::ATriangle()
    : APrimitiveActor()
{
    InitializeActor();
}

ATriangle::ATriangle(const FUObjectInitializer& ObjectInitializer)
    : APrimitiveActor(ObjectInitializer)
{
    InitializeActor();
}

ATriangle::~ATriangle()
{
}

const char* ATriangle::GetObjClassName() const
{
    return "ATriangle";
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
    StaticMeshComponent = new UStaticMeshComponent();
    SetPrimitiveComponent(StaticMeshComponent);
}