#include "DecalComponent.h"

#include "Core/ResourceManager.h"

DEFINE_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
	const TArray<FString> MatNames = FResourceManager::Get().GetMaterialNames();
	SetMaterial(FResourceManager::Get().FindMaterial(MatNames[0]));
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
	NewComp->DecalSize = this->DecalSize;

	NewComp->DuplicateSubObjects();

	return NewComp;
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Size", EPropertyType::Vec3, &DecalSize });
	OutProps.push_back({ "Color", EPropertyType::Vec4, &DecalColor });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UDecalComponent::UpdateWorldAABB() const
{
	// 월드 공간에서의 AABB 계산
	FVector WorldLocation = GetWorldLocation();
	FVector HalfSize = DecalSize * 0.5f;
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
	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
	return WorldMatrix;
}
