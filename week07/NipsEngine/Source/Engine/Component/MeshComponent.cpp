#include "MeshComponent.h"
#include "Render/Resource/Material.h"

DEFINE_CLASS(UMeshComponent, UPrimitiveComponent)

// UpdateWorldAABB 등의 함수를 오버라이드하지 않았기 때문에 UMeshComponent도 추상 클래스가 됩니다.
// 추후에 MeshComponent를 사용할 일이 있다면 Duplicate의 주석을 해제하고 수정하시면 됩니다.

// 부모 클래스를 계층별로 복사한 뒤, Matrerial을 얕은 복사로 세팅해 줍니다.
//UMeshComponent* UMeshComponent::Duplicate()
//{
//    UMeshComponent* NewComp = UObjectManager::Get().CreateObject<UMeshComponent>();
//
//    NewComp->SetActive(this->IsActive());
//    NewComp->SetOwner(nullptr);
//    
//    NewComp->SetRelativeLocation(this->GetRelativeLocation());
//    NewComp->SetRelativeRotation(this->GetRelativeRotation());
//    NewComp->SetRelativeScale(this->GetRelativeScale());
//
//    NewComp->SetVisibility(this->IsVisible());
//  
//    NewComp->OverrideMaterial = this->OverrideMaterial;
//    NewComp->ScrollUV = this->ScrollUV;
//
//    return NewComp;
//}

void UMeshComponent::SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial)
{
	if (SlotIndex < 0)
	{
		return;
	}
	
	if (SlotIndex >= static_cast<int32>(OverrideMaterial.size()))
	{
		OverrideMaterial.resize(SlotIndex + 1, nullptr);
	}

	OverrideMaterial[SlotIndex] = InMaterial;
}

UMaterialInterface* UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(OverrideMaterial.size()))
	{
		return nullptr;
	}
	
	return OverrideMaterial[SlotIndex];
}

const TArray<UMaterialInterface*>& UMeshComponent::GetOverrideMaterial() const
{
	return OverrideMaterial;
}

int32 UMeshComponent::GetNumMaterials() const
{
	return static_cast<int32>(OverrideMaterial.size());
}

void UMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Scroll U", EPropertyType::Float, &ScrollUV.first,  -1.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Scroll V", EPropertyType::Float, &ScrollUV.second, -1.0f, 1.0f, 0.01f });
}

void UMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UMeshComponent::TickComponent(float DeltaTime)
{
	//ScrollUV.second += DeltaTime;

	//if (ScrollUV.first >= 1.f) ScrollUV.first = 0.f;
}

