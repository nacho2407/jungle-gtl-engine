#include "Component/UPrimitiveComponent.h"

UPrimitiveComponent::UPrimitiveComponent() 
	: bVisible(true) {}

UPrimitiveComponent::~UPrimitiveComponent() {}

void UPrimitiveComponent::BeginPlay()
{
	USceneComponent::BeginPlay(); // @@ 이렇게 하면 뭐가 됨? USceneComponent의 BeginPlay 아무것도 안 하지 않나?
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime); // @@ 이렇게 하면 뭐가 됨? USceneComponent의 TickComponent 아무것도 안 하지 않나?
}

void UPrimitiveComponent::SetVisibility(bool bInVisible)
{
	bVisible = bInVisible;
}

bool UPrimitiveComponent::IsVisible() const
{
	return bVisible;
}

FRenderItem UPrimitiveComponent::CreateRenderItem() const
{
	FRenderItem Item;
	Item.WorldMatrix = GetWorldTransformMatrix();
	Item.Mesh = nullptr;
	Item.Color = FVector4(1, 1, 1, 1);
	Item.CullMode = ERenderCullMode::Back; // @@@ 뭐하는 놈일까
	Item.bDepthEnable = true; // @@@ 뭐하는 놈일까
	return Item;
}