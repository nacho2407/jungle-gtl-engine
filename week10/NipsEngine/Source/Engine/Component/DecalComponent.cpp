#include "DecalComponent.h"

#include "Core/ResourceManager.h"

DEFINE_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
	: Material(nullptr), Size(5.0f, 5.0f, 5.0f)
{
	SetMaterial(FResourceManager::Get().FindMaterial(""));
}

UDecalComponent* UDecalComponent::Duplicate()
{
	UDecalComponent* NewComp = UObjectManager::Get().CreateObject<UDecalComponent>();
	NewComp->SetActive(this->IsActive());
	NewComp->SetOwner(nullptr);
	
	NewComp->SetRelativeLocation(this->GetRelativeLocation());
	NewComp->SetRelativeRotation(this->GetRelativeRotation());
	NewComp->SetRelativeScale(this->GetRelativeScale());
	
	NewComp->SetVisibility(this->IsVisible());

	NewComp->Material = this->Material;
	NewComp->Size = this->Size;

	NewComp->DuplicateSubObjects();

	return NewComp;
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Material", EPropertyType::Name, &Material });
	OutProps.push_back({ "Size", EPropertyType::Vec3, &Size });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UDecalComponent::UpdateWorldAABB() const
{
	// 월드 공간에서의 AABB 계산
	FVector WorldLocation = GetWorldLocation();
	FVector HalfSize = Size * 0.5f;
	WorldAABB.Min = WorldLocation - HalfSize;
	WorldAABB.Max = WorldLocation + HalfSize;
}

bool UDecalComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	// TODO: OBB Intersection or billboard intersection
	return false;
}

FMatrix UDecalComponent::GetDecalMatrix() const
{
	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(Size) * GetWorldMatrix();
	return WorldMatrix;
}
