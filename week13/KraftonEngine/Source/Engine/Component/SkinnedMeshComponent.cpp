#include "SkinnedMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Serialization/Archive.h"
#include "Runtime/Engine.h"
#include "Mesh/FbxManager.h"

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)
HIDE_FROM_COMPONENT_LIST(USkinnedMeshComponent)

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	SkeletalMesh = InMesh;
	if (InMesh)
	{
		SkeletalMeshPath = InMesh->GetAssetPathFileName();
		const TArray<FSkeletalMaterial>& DefaultMaterials = SkeletalMesh->GetSkeletalMaterials();

		OverrideMaterials.resize(DefaultMaterials.size());
		MaterialSlots.resize(DefaultMaterials.size());

		for (int32 i = 0; i < (int32)DefaultMaterials.size(); ++i)
		{
			OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;

			if (OverrideMaterials[i])
				MaterialSlots[i].Path = OverrideMaterials[i]->GetAssetPathFileName();
			else
				MaterialSlots[i].Path = "None";
		}
	}
	else
	{
		SkeletalMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!SkeletalMesh) return;
	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->Vertices.empty()) return;

	if (!Asset->bBoundsValid)
	{
		Asset->CacheBounds();
	}

	CachedLocalCenter = Asset->BoundsCenter;
	CachedLocalExtent = Asset->BoundsExtent;
	bHasValidBounds = Asset->bBoundsValid;
}

USkeletalMesh* USkinnedMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh;
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
	if (!bHasValidBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

	float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X
			 + std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.Z;
	float Ey = std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.X
			 + std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Z;
	float Ez = std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.X
			 + std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << SkeletalMeshPath;
}

void USkinnedMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Loaded = FFbxManager::LoadFbxSkeletalMesh(SkeletalMeshPath, Device);
		if (Loaded)
		{
			SetSkeletalMesh(Loaded);
		}
	}

	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UMeshComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, "Mesh", &SkeletalMeshPath });
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			USkeletalMesh* Loaded = FFbxManager::LoadFbxSkeletalMesh(SkeletalMeshPath, Device);
			SetSkeletalMesh(Loaded);
		}
		else
		{
			SetSkeletalMesh(nullptr);
		}
		MarkWorldBoundsDirty();
	}
}
