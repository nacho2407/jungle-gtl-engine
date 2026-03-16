#include "Actor/ACube.h"
#include "Component/UStaticMeshComponent.h"
#include "Resource/UStaticMesh.h"

ACube::ACube()
    : APrimitiveActor()
{
    InitializeActor();
}

ACube::ACube(const FUObjectInitializer& ObjectInitializer)
    : APrimitiveActor(ObjectInitializer)
{
    InitializeActor();
}

ACube::~ACube()
{
}

const char* ACube::GetObjClassName() const
{
    return "ACube";
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
    StaticMeshComponent = new UStaticMeshComponent();
    SetPrimitiveComponent(StaticMeshComponent);
}
