#include "MeshComponent.h"

#include "Render/Scene/Scene.h"

void UMeshComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
}

void UMeshComponent::SetMaterial(int32 SlotIndex, UMaterialInterface* InMaterial)
{
	if (SlotIndex < 0)
	{
		return;
	}

	if (SlotIndex >= static_cast<int32>(Materials.size()))
	{
		Materials.resize(SlotIndex + 1, nullptr);
	}

	Materials[SlotIndex] = InMaterial;
	UPrimitiveComponent::MarkRenderStateDirty(ESceneProxyDirtyFlag::Material);
}

UMaterialInterface* UMeshComponent::GetMaterial(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(Materials.size()))
	{
		return nullptr;
	}

	return Materials[SlotIndex];
}

const TArray<UMaterialInterface*>& UMeshComponent::GetOverrideMaterial() const
{
	return Materials;
}

int32 UMeshComponent::GetNumMaterials() const
{
	return static_cast<int32>(Materials.size());
}

void UMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UMeshComponent::TickComponent(float DeltaTime)
{
	(void)DeltaTime;
}
