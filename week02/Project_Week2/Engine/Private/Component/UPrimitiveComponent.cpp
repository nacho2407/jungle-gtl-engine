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

	Item.HitProxy.Type = EHitProxyType::Primitive;
	Item.HitProxy.Primitive = const_cast<UPrimitiveComponent*>(this);
	Item.HitProxy.Axis = EGizmoAxis::None;

	Item.Color = RenderColor;
	Item.CullMode = CullMode; // @@@ front/back
	Item.bDepthEnable = bDepthEnable; // @@@ 
	Item.bUseVertexColor = bUseVertexColor;
	return Item;
}

void UPrimitiveComponent::SetRenderColor(const FVector4& InColor)
{
	RenderColor = InColor;
}

const FVector4& UPrimitiveComponent::GetRenderColor() const
{
	return RenderColor;
}

void UPrimitiveComponent::SetDepthEnable(bool bInDepthEnable)
{
	bDepthEnable = bInDepthEnable;
}

void UPrimitiveComponent::SetCullMode(ERenderCullMode InCullMode)
{
	CullMode = InCullMode;
}

void UPrimitiveComponent::SetUseVertexColor(bool bInUseVertexColor)
{
	bUseVertexColor = bInUseVertexColor;
}