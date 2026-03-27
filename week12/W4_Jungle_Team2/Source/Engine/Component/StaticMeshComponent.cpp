#include "StaticMeshComponent.h"

DEFINE_CLASS(UStaticMeshComponent, UMeshComponent)

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	if (StaticMeshAsset == InStaticMesh)
	{
		return;
	}

	StaticMeshAsset = InStaticMesh;

	MarkBoundsDirty();
	MarkRenderStateDirty();
}

UStaticMesh* UStaticMeshComponent::GetStaticMesh()
{
	return StaticMeshAsset;
}

bool UStaticMeshComponent::HasValidMesh() const
{
	return StaticMeshAsset != nullptr && StaticMeshAsset->HasValidMeshData();
}

void UStaticMeshComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();

	if (!HasValidMesh())
	{
		bBoundsDirty = false;
		return;
	}

	const FAABB& LocalBounds = StaticMeshAsset->GetLocalBounds();
	if (!LocalBounds.IsValid())
	{
		bBoundsDirty = false;
		return;
	}

	const FVector LocalCorners[8] =
	{
		FVector{LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Min.Z},
		FVector{LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Min.Z},
		FVector{LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Min.Z},
		FVector{LocalBounds.Min.X, LocalBounds.Min.Y, LocalBounds.Max.Z},
		FVector{LocalBounds.Min.X, LocalBounds.Max.Y, LocalBounds.Max.Z},
		FVector{LocalBounds.Max.X, LocalBounds.Min.Y, LocalBounds.Max.Z},
		FVector{LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Min.Z},
		FVector{LocalBounds.Max.X, LocalBounds.Max.Y, LocalBounds.Max.Z}
	};
	
	const FMatrix WorldMatrix = GetComponentTransform().ToMatrix();
	
	for (const FVector& Corner : LocalCorners)
	{
		const FVector WorldPos = WorldMatrix.TransformPositionWithW(Corner);
		WorldAABB.Expand(WorldPos);
	}
	
	bBoundsDirty = false;
}

bool UStaticMeshComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!HasValidMesh())
	{
		return false;
	}

	EnsureBoundsUpdated();

	float BoxT = 0.0f;
	if (!WorldAABB.IntersectRay(Ray, BoxT))
	{
		return false;
	}

	const TArray<FNormalVertex>& Vertices = StaticMeshAsset->GetVertices();
	const TArray<uint32>& Indices = StaticMeshAsset->GetIndices();

	if (Vertices.empty() || Indices.size() < 3)
	{
		return false;
	}

	const FMatrix WorldMatrix = GetComponentTransform().ToMatrix();

	bool bHit = false;
	float ClosestDistance = FLT_MAX;
	FHitResult BestHit;

	for (size_t i = 0; i + 2 < Indices.size(); i += 3)
	{
		const uint32 I0 = Indices[i];
		const uint32 I1 = Indices[i + 1];
		const uint32 I2 = Indices[i + 2];

		if (I0 >= Vertices.size() || I1 >= Vertices.size() || I2 >= Vertices.size())
		{
			continue;
		}

		const FVector V0 = WorldMatrix.TransformPosition(Vertices[I0].Position);
		const FVector V1 = WorldMatrix.TransformPosition(Vertices[I1].Position);
		const FVector V2 = WorldMatrix.TransformPosition(Vertices[I2].Position);

		FHitResult Hit;
		if (IntersectTriangle(Ray, V0, V1, V2, Hit))
		{
			if (Hit.Distance < ClosestDistance)
			{
				ClosestDistance = Hit.Distance;
				BestHit = Hit;
				bHit = true;
			}
		}
	}

	if (bHit)
	{
		BestHit.HitComponent = this;
		OutHitResult = BestHit;
	}

	return bHit;
}

const FAABB& UStaticMeshComponent::GetWorldAABB() const
{
	EnsureBoundsUpdated();
	return WorldAABB;
}

void UStaticMeshComponent::OnTransformChanged()
{
	UMeshComponent::OnTransformChanged();

	MarkBoundsDirty();
	MarkRenderStateDirty();
}

bool UStaticMeshComponent::ConsumRenderStateDirty()
{
	const bool bWasDirty = bRenderStateDirty;
	bRenderStateDirty = false;
	return bWasDirty;
}

void UStaticMeshComponent::MarkBoundsDirty()
{
	bBoundsDirty = true;
}

void UStaticMeshComponent::MarkRenderStateDirty()
{
	bRenderStateDirty = true;
}

void UStaticMeshComponent::EnsureBoundsUpdated() const
{
	if (!bBoundsDirty)
	{
		return;
	}

	const_cast<UStaticMeshComponent*>(this)->UpdateWorldAABB();
}
