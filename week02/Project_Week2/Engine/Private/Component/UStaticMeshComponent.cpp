#include "Component/UStaticMeshComponent.h"

UStaticMeshComponent::UStaticMeshComponent()
    : StaticMesh(nullptr)
{
}

UStaticMeshComponent::~UStaticMeshComponent()
{
}

void UStaticMeshComponent::BeginPlay()
{
    UPrimitiveComponent::BeginPlay();
}

void UStaticMeshComponent::TickComponent(float DeltaTime)
{
    UPrimitiveComponent::TickComponent(DeltaTime);
}

const char* UStaticMeshComponent::GetObjClassName() const
{
    return "UStaticMeshComponent";
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InMesh)
{
    StaticMesh = InMesh;
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh() const
{
    return StaticMesh;
}

FRenderItem UStaticMeshComponent::CreateRenderItem() const
{
    FRenderItem Item = UPrimitiveComponent::CreateRenderItem();
    Item.Mesh = StaticMesh;
    return Item;
}