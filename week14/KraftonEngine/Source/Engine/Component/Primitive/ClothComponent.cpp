#include "Component/Primitive/ClothComponent.h"

#include "GameFramework/World.h"
#include "Physics/PhysicsScene.h"
#include "Render/Proxy/ClothSceneProxy.h"

namespace
{
	bool IsClothTopologyProperty(const char* PropName)
	{
		return strcmp(PropName, "Width") == 0 ||
			strcmp(PropName, "Height") == 0 ||
			strcmp(PropName, "Spacing") == 0 ||
			strcmp(PropName, "PinMode") == 0 ||
			strcmp(PropName, "Pin Mode") == 0 ||
			strcmp(PropName, "bUseTether") == 0 ||
			strcmp(PropName, "Use Tether") == 0 ||
			strcmp(PropName, "bUseShear") == 0 ||
			strcmp(PropName, "Use Shear") == 0 ||
			strcmp(PropName, "bUseBending") == 0 ||
			strcmp(PropName, "Use Bending") == 0;
	}

	bool IsClothRuntimeProperty(const char* PropName)
	{
		return strcmp(PropName, "Gravity") == 0 ||
			strcmp(PropName, "SolverFrequency") == 0 ||
			strcmp(PropName, "Solver Frequency") == 0 ||
			strcmp(PropName, "StiffnessFrequency") == 0 ||
			strcmp(PropName, "Stiffness Frequency") == 0 ||
			strcmp(PropName, "SubstepCount") == 0 ||
			strcmp(PropName, "Substep Count") == 0 ||
			strcmp(PropName, "bEnableCCD") == 0 ||
			strcmp(PropName, "Enable CCD") == 0 ||
			strcmp(PropName, "Damping") == 0 ||
			strcmp(PropName, "LinearDrag") == 0 ||
			strcmp(PropName, "Linear Drag") == 0 ||
			strcmp(PropName, "AngularDrag") == 0 ||
			strcmp(PropName, "Angular Drag") == 0 ||
			strcmp(PropName, "StructuralStiffness") == 0 ||
			strcmp(PropName, "Structural Stiffness") == 0 ||
			strcmp(PropName, "ShearStiffness") == 0 ||
			strcmp(PropName, "Shear Stiffness") == 0 ||
			strcmp(PropName, "BendingStiffness") == 0 ||
			strcmp(PropName, "Bending Stiffness") == 0 ||
			strcmp(PropName, "bEnableWorldCollision") == 0 ||
			strcmp(PropName, "Enable World Collision") == 0 ||
			strcmp(PropName, "CollisionBoundsPadding") == 0 ||
			strcmp(PropName, "Collision Bounds Padding") == 0 ||
			strcmp(PropName, "CollisionMassScale") == 0 ||
			strcmp(PropName, "Collision Mass Scale") == 0 ||
			strcmp(PropName, "Friction") == 0 ||
			strcmp(PropName, "RenderNormalOffset") == 0 ||
			strcmp(PropName, "Render Normal Offset") == 0;
	}
}

UClothComponent::UClothComponent()
{
	SetCastShadow(true);
	SetCastShadowAsTwoSided(true);

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

	if (strcmp(PropertyName, "ClothDesc") == 0 ||
		strcmp(PropertyName, "Cloth Setup") == 0 ||
		IsClothTopologyProperty(PropertyName) ||
		IsClothRuntimeProperty(PropertyName))
	{
		RebuildCloth(true);
	}
}

void UClothComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
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
