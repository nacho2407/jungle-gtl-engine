#include "StaticMeshComponent.h"
#include "Object/Class.h"
#include "PrimitiveComponent.h"
#include "MeshComponent.h"
#include "Asset/ObjManager.h"
IMPLEMENT_RTTI(UStaticMeshComponent, UMeshComponent)

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	StaticMesh = InStaticMesh;

	if (StaticMesh)
	{
		int32 NeededMaterialSlots = StaticMesh->GetNumSections();
		Materials.resize(NeededMaterialSlots, nullptr);
		const TArray<std::shared_ptr<FMaterial>>& DefaultMats = StaticMesh->GetDefaultMaterials();
		for (int32 i = 0; i < NeededMaterialSlots && i < DefaultMats.size(); ++i)
		{
			Materials[i] = DefaultMats[i];
		}
		UpdateBounds();
	}
	else
	{
		Materials.clear();
	}
}

FRenderMesh* UStaticMeshComponent::GetRenderMesh() const
{
	 return StaticMesh ? StaticMesh->GetRenderData() : nullptr;
}

FBoxSphereBounds UStaticMeshComponent::GetLocalBounds() const
{
	if (StaticMesh)
	{
		return StaticMesh->LocalBounds;
	}
	return UPrimitiveComponent::GetLocalBounds();
}

FBoxSphereBounds UStaticMeshComponent::CalcBounds(const FMatrix& LocalToWorld) const
{
	return UPrimitiveComponent::CalcBounds(LocalToWorld);
}

void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	// UMeshComponent::Serialize(Ar);
	FString AssetName;
	if (Ar.IsSaving())
	{
		if (StaticMesh)
		{
			AssetName = StaticMesh->GetAssetPathFileName();
		}
		Ar.Serialize("ObjStaticMeshAsset", AssetName);
	}
	else
	{
		Ar.Serialize("ObjStaticMeshAsset", AssetName);
		if (!AssetName.empty())
		{
			SetStaticMesh(FObjManager::LoadObjStaticMeshAsset(AssetName));
		}
	}
}