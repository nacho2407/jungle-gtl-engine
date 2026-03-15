#include "Component/UStaticMeshComponent.h"

UStaticMeshComponent::UStaticMeshComponent()
    : MeshName("")
{
    SetPrimitiveType(EPrimitiveType::StaticMesh);
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

void UStaticMeshComponent::SetMeshName(const std::string& InMeshName)
{
    MeshName = InMeshName;
}

const std::string& UStaticMeshComponent::GetMeshName() const
{
    return MeshName;
}

FRenderItem UStaticMeshComponent::CreateRenderItem() const
{
    FRenderItem Item = UPrimitiveComponent::CreateRenderItem();
    Item.MeshName = MeshName;
    return Item;
}