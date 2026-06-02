#include "Component/Primitive/ClothComponent.h"

#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Physics/PhysicsScene.h"
#include "Render/Proxy/ClothSceneProxy.h"

UClothComponent::UClothComponent()
{
	SetCastShadow(true);
	SetCastShadowAsTwoSided(true);

	if (!MaterialSlot.empty() && MaterialSlot != "None")
	{
		UMaterialInterface* LoadedMat = FMaterialManager::Get().GetOrCreateMaterialInterface(MaterialSlot);

		if (LoadedMat)
		{
			SetMaterial(LoadedMat);
		}
	}

	RebuildCloth(false);
}

UClothComponent::~UClothComponent()
{
	ClothInstance.Release();
}

FPrimitiveSceneProxy* UClothComponent::CreateSceneProxy()
{
	return new FClothSceneProxy(this);
}

void UClothComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		World->RegisterClothComponent(this);
	}
}

void UClothComponent::EndPlay()
{
	UPrimitiveComponent::EndPlay();

	if (UWorld* World = GetWorld())
	{
		World->UnregisterClothComponent(this);
	}
}

void UClothComponent::TickClothPostPhysics(float DeltaTime)
{
	UpdateClothWorldCollision();
	ClothInstance.Simulate(DeltaTime, ClothDesc.SubstepCount, ClothDesc.RenderNormalOffset);

	MarkWorldBoundsDirty();
}

void UClothComponent::UpdateWorldAABB() const
{
	const TArray<FVertexPNCTT>& Vertices = ClothInstance.GetRenderVertices();
	if (Vertices.empty())
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldMin = CachedWorldMatrix.TransformPositionWithW(Vertices[0].Position);
	FVector WorldMax = WorldMin;

	for (const FVertexPNCTT& Vertex : Vertices)
	{
		const FVector P = CachedWorldMatrix.TransformPositionWithW(Vertex.Position);

		WorldMin.X = std::min(WorldMin.X, P.X);
		WorldMin.Y = std::min(WorldMin.Y, P.Y);
		WorldMin.Z = std::min(WorldMin.Z, P.Z);

		WorldMax.X = std::max(WorldMax.X, P.X);
		WorldMax.Y = std::max(WorldMax.Y, P.Y);
		WorldMax.Z = std::max(WorldMax.Z, P.Z);
	}

	WorldAABBMinLocation = WorldMin;
	WorldAABBMaxLocation = WorldMax;
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UClothComponent::UpdateClothWorldCollision()
{
	if (!ClothDesc.bEnableWorldCollision)
	{
		ClothInstance.UpdateCollision(FClothCollisionData());
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->GetPhysicsScene()) return;

	FClothCollisionGatherParams Params;
	Params.WorldBounds = GetWorldBoundingBox();
	Params.WorldToCloth = GetWorldInverseMatrix();
	Params.ClothChannel = GetCollisionObjectType();
	Params.BoundsPadding = ClothDesc.CollisionBoundsPadding;
	Params.IgnoreActor = nullptr;
	Params.IgnoreComponent = this;

	FClothCollisionData CollisionData;
	World->GetPhysicsScene()->GatherClothCollision(Params, CollisionData);

	ClothInstance.UpdateCollision(CollisionData);
}

void UClothComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (!PropertyName) return;

	if (strcmp(PropertyName, "MaterialSlot") == 0 ||
		strcmp(PropertyName, "Material") == 0)
	{
		if (MaterialSlot.empty() || MaterialSlot == "None")
		{
			SetMaterial(nullptr);
		}
		else
		{
			UMaterialInterface* LoadedMat = FMaterialManager::Get().GetOrCreateMaterialInterface(MaterialSlot);

			if (LoadedMat)
			{
				SetMaterial(LoadedMat);
			}
		}
		return;
	}

	if (strcmp(PropertyName, "ClothDesc") == 0 ||
		strcmp(PropertyName, "Cloth Setup") == 0)
	{
		RebuildCloth(true);
	}
}

void UClothComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	if (!MaterialSlot.empty() && MaterialSlot != "None")
	{
		UMaterialInterface* LoadedMat = FMaterialManager::Get().GetOrCreateMaterialInterface(MaterialSlot);

		if (LoadedMat)
		{
			SetMaterial(LoadedMat);
		}
	}
	else
	{
		SetMaterial(nullptr);
	}

	RebuildCloth(true);
}

void UClothComponent::RebuildCloth(bool bRecreateRenderState)
{
	ClothInstance.Release();
	ClothInstance.InitializeGrid(ClothDesc);

	if (bRecreateRenderState)
	{
		MarkRenderStateDirty();
	}

	MarkWorldBoundsDirty();
}

void UClothComponent::SetMaterial(UMaterialInterface* InMaterial)
{
	Material = InMaterial;

	if (Material)
	{
		MaterialSlot = Material->GetAssetPathFileName();
	}
	else
	{
		MaterialSlot = "None";
	}

	MarkProxyDirty(EDirtyFlag::Material);
}
