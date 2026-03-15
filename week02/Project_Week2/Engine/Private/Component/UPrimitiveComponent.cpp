#include "Component/UPrimitiveComponent.h"

UPrimitiveComponent::UPrimitiveComponent() 
	: bVisible(true), PrimitiveType(EPrimitiveType::None) {}

UPrimitiveComponent::~UPrimitiveComponent() {}

void UPrimitiveComponent::BeginPlay()
{
	USceneComponent::BeginPlay(); // 이렇게 하면 뭐가 됨? USceneComponent의 BeginPlay 아무것도 안 하지 않나?
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime); // 이렇게 하면 뭐가 됨? USceneComponent의 TickComponent 아무것도 안 하지 않나?
}

void UPrimitiveComponent::SetVisibility(bool bInVisible)
{
	bVisible = bInVisible;
}

bool UPrimitiveComponent::IsVisible() const
{
	return bVisible;
}

void UPrimitiveComponent::SetPrimitiveType(EPrimitiveType InType)
{
	PrimitiveType = InType;
}

EPrimitiveType UPrimitiveComponent::GetPrimitiveType() const
{
	return PrimitiveType;
}

FRenderItem UPrimitiveComponent::CreateRenderItem() const
{
	FRenderItem Item;
	Item.WorldMatrix = GetWorldTransformMatrix();
	Item.PrimitiveType = PrimitiveType;
	return Item;
}