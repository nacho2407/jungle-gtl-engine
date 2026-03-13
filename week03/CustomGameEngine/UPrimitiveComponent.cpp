#include "UPrimitiveComponent.h"
#include "URender.h"

UPrimitiveComponent::UPrimitiveComponent()
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

void UPrimitiveComponent::Update(float DeltaTime)
{

}

URenderObject* UPrimitiveComponent::Render()
{
	return nullptr;
}

REGISTER_CLASS(UPrimitiveComponent);
