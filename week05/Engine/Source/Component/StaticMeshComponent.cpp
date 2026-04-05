#include "StaticMeshComponent.h"

#include <filesystem>

#include "Object/Class.h"
#include "PrimitiveComponent.h"
#include "MeshComponent.h"
#include "Asset/ObjManager.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Renderer/Material.h"
#include "Renderer/SceneProxy.h"
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
	}
	else
	{
		Materials.clear();
	}
	UpdateBounds();
	MarkRenderStateDirty();
}

FRenderMesh* UStaticMeshComponent::GetRenderMesh() const
{
	return StaticMesh ? StaticMesh->GetRenderData() : nullptr;
}

FRenderMesh* UStaticMeshComponent::GetRenderMesh(const float& Distance) const
{
	return StaticMesh ? StaticMesh->GetRenderDataForDistance(Distance) : nullptr;
}

std::shared_ptr<FPrimitiveSceneProxy> UStaticMeshComponent::CreateSceneProxy() const
{
	return std::make_shared<FStaticMeshSceneProxy>(this);
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

bool UStaticMeshComponent::IntersectLocalRay(const FVector& LocalOrigin, const FVector& LocalDir, float& InOutDist) const
{
	return StaticMesh && StaticMesh->IntersectLocalRay(LocalOrigin, LocalDir, InOutDist);
}

void UStaticMeshComponent::Serialize(FArchive& Ar)
{

	if (Ar.IsSaving())
	{
		UMeshComponent::Serialize(Ar);

		FString MeshFileName = "";
		if (StaticMesh)
		{
			MeshFileName = FPaths::FromPath(FPaths::ToPath(StaticMesh->GetAssetPathFileName()).filename());
		}
		Ar.Serialize("ObjStaticMeshAsset", MeshFileName);
	}
	else
	{
		if (Ar.Contains("ObjStaticMeshAsset"))
		{
			FString MeshFileName;
			Ar.Serialize("ObjStaticMeshAsset", MeshFileName);

			if (!MeshFileName.empty())
			{
				UStaticMesh* LoadedMesh = FObjManager::LoadStaticMeshAsset(MeshFileName);
				SetStaticMesh(LoadedMesh);
			}
		}
		UMeshComponent::Serialize(Ar);
	}
}
