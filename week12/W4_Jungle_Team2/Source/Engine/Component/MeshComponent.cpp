#include "MeshComponent.h"

DEFINE_CLASS(UMeshComponent, UPrimitiveComponent)

void UMeshComponent::SetMaterial(int32 SlotIndex, FMaterial* InMaterial)
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

FMaterial* UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(OverrideMaterial.size()))
	{
		return nullptr;
	}
	
	return OverrideMaterial[SlotIndex];
}

int32 UMeshComponent::GetMaterialCount() const
{
	return static_cast<int32>(OverrideMaterial.size());
}

void UMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	
	//	지금은 Override Material 자체를 에디터에 노출하지 않음
	//	TODO : Material Asset System 정리되면 슬롯별로 노출

}

void UMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

