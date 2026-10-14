#include "Component/Primitive/ClothComponent.h"

#include "Component/Shape/CapsuleComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Component/Primitive/SkinnedMeshComponent.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Mesh/MeshManager.h"
#include "Physics/PhysicsScene.h"
#include "Profiling/Stats/PhysicsStats.h"
#include "Profiling/Stats/Stats.h"
#include "Render/Proxy/ClothSceneProxy.h"
#include "Runtime/Engine.h"

#include <cmath>

namespace
{
	constexpr float ClothMatrixDecomposeTolerance = 1.0e-6f;

	FTransform MatrixToClothEditorTransform(const FMatrix& Matrix)
	{
		FTransform Result;
		Result.Location = Matrix.GetLocation();
		Result.Scale = Matrix.GetScale();

		FMatrix RotationMatrix = Matrix;
		RotationMatrix.M[3][0] = 0.0f;
		RotationMatrix.M[3][1] = 0.0f;
		RotationMatrix.M[3][2] = 0.0f;
		RotationMatrix.M[3][3] = 1.0f;

		if (std::fabs(Result.Scale.X) > ClothMatrixDecomposeTolerance)
		{
			RotationMatrix.M[0][0] /= Result.Scale.X;
			RotationMatrix.M[0][1] /= Result.Scale.X;
			RotationMatrix.M[0][2] /= Result.Scale.X;
		}

		if (std::fabs(Result.Scale.Y) > ClothMatrixDecomposeTolerance)
		{
			RotationMatrix.M[1][0] /= Result.Scale.Y;
			RotationMatrix.M[1][1] /= Result.Scale.Y;
			RotationMatrix.M[1][2] /= Result.Scale.Y;
		}

		if (std::fabs(Result.Scale.Z) > ClothMatrixDecomposeTolerance)
		{
			RotationMatrix.M[2][0] /= Result.Scale.Z;
			RotationMatrix.M[2][1] /= Result.Scale.Z;
			RotationMatrix.M[2][2] /= Result.Scale.Z;
		}

		Result.Rotation = RotationMatrix.ToQuat().GetNormalized();
		return Result;
	}
}

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

void UClothComponent::PrepareClothSimulation()
{
	{
		SCOPE_STAT_CAT("Cloth_BoneAttachment", "Cloth");
		UpdateBoneAttachment();
	}

	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FVector CurrentLocation = WorldMatrix.GetLocation();
	const FQuat CurrentRotation = WorldMatrix.ToQuat().GetNormalized();

	const bool bTeleport = !bHasPreviousSimulationTransform ||
		FVector::Distance(CurrentLocation, PreviousSimulationLocation) > ClothDesc.TeleportDistanceThreshold;

	ClothInstance.SetSimulationSpaceTransform(CurrentLocation, CurrentRotation, bTeleport);

	PreviousSimulationLocation = CurrentLocation;
	PreviousSimulationRotation = CurrentRotation;
	bHasPreviousSimulationTransform = true;

	{
		SCOPE_STAT_CAT("Cloth_WorldCollision", "Cloth");
		UpdateClothWorldCollision();
	}

	if (ClothInstance.IsInitialized())
	{
		PHYSICS_STATS_RECORD_CLOTH_ACTIVE(
			ClothInstance.GetParticleCount(),
			ClothInstance.GetTriangleCount(),
			static_cast<uint32>((std::max)(1, ClothDesc.SubstepCount)));
	}
}

void UClothComponent::FinalizeClothSimulation()
{
	ClothInstance.UpdateRenderData(ClothDesc.RenderNormalOffset);
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

	if (bIgnoreOwnerCapsuleCollision)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			if (UCapsuleComponent* OwnerCapsule = OwnerActor->GetComponentByClass<UCapsuleComponent>())
			{
				Params.IgnoreComponents.push_back(OwnerCapsule);
			}
		}
	}

	FClothCollisionData CollisionData;
	World->GetPhysicsScene()->GatherClothCollision(Params, CollisionData);

	ClothInstance.UpdateCollision(CollisionData);
	PHYSICS_STATS_RECORD_CLOTH_COLLISION(
		static_cast<uint32>(CollisionData.Spheres.size()),
		static_cast<uint32>(CollisionData.Capsules.size() / 2),
		static_cast<uint32>(CollisionData.Planes.size()),
		static_cast<uint32>(CollisionData.ConvexMasks.size()));
}

void UClothComponent::UpdateBoneAttachment()
{
	if (!bAttachToOwnerMeshBone || bEditAttachOffset || AttachBoneName.empty()) return;

	USkinnedMeshComponent* TargetMeshComponent = ResolveAttachMeshComponent();
	if (!TargetMeshComponent) return;

	const FTransform AttachLocalOffset(AttachOffsetLocation, AttachOffsetRotation, AttachOffsetScale);

	FTransform SocketWorldTransform;
	if (!TargetMeshComponent->GetBoneSocketWorldTransform(AttachBoneName, AttachLocalOffset, SocketWorldTransform))
	{
		return;
	}

	FTransform TargetRelativeTransform = SocketWorldTransform;
	if (USceneComponent* ParentComponent = GetParent())
	{
		const FMatrix TargetRelativeMatrix = SocketWorldTransform.ToMatrix() * ParentComponent->GetWorldMatrix().GetInverse();
		TargetRelativeTransform = MatrixToClothEditorTransform(TargetRelativeMatrix);
	}

	SetRelativeTransform(TargetRelativeTransform);
}

USkinnedMeshComponent* UClothComponent::ResolveAttachMeshComponent()
{
	if (!AttachMeshComponent)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			AttachMeshComponent = OwnerActor->GetComponentByClass<USkinnedMeshComponent>();
		}
	}

	return AttachMeshComponent;
}

void UClothComponent::CaptureAttachOffsetFromCurrentTransform()
{
	bCaptureCurrentAttachOffset = false;

	if (!bAttachToOwnerMeshBone || AttachBoneName.empty()) return;

	USkinnedMeshComponent* TargetMeshComponent = ResolveAttachMeshComponent();
	if (!TargetMeshComponent) return;

	FMatrix BoneWorldMatrix;
	if (!TargetMeshComponent->GetBoneWorldMatrixByName(AttachBoneName, BoneWorldMatrix))
	{
		return;
	}

	const FMatrix OffsetMatrix = GetWorldMatrix() * BoneWorldMatrix.GetInverse();
	const FTransform OffsetTransform = MatrixToClothEditorTransform(OffsetMatrix);

	AttachOffsetLocation = OffsetTransform.Location;
	AttachOffsetRotation = OffsetTransform.Rotation.ToRotator();
	AttachOffsetScale = FVector(1.0f, 1.0f, 1.0f);
	bEditAttachOffset = false;

	UpdateBoneAttachment();
	RebuildCloth(true);
}

void UClothComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (!PropertyName) return;

	if (strcmp(PropertyName, "ClothMeshPath") == 0 ||
		strcmp(PropertyName, "Cloth Mesh") == 0)
	{
		if (ClothMeshPath.empty() || ClothMeshPath == "None")
		{
			SetClothMesh(nullptr);
		}
		else
		{
			ID3D11Device* Device = GEngine
				? GEngine->GetRenderer().GetFD3DDevice().GetDevice()
				: nullptr;

			UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(ClothMeshPath, Device);
			SetClothMesh(Loaded);
		}

		return;
	}

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

	if (strcmp(PropertyName, "bCaptureCurrentAttachOffset") == 0 ||
		strcmp(PropertyName, "Capture Current Attach Offset") == 0)
	{
		if (bCaptureCurrentAttachOffset)
		{
			CaptureAttachOffsetFromCurrentTransform();
		}
		else
		{
			bCaptureCurrentAttachOffset = false;
		}

		return;
	}

	if (strcmp(PropertyName, "bAttachToOwnerMeshBone") == 0 ||
		strcmp(PropertyName, "Attach To Owner Mesh Bone") == 0 ||
		strcmp(PropertyName, "bEditAttachOffset") == 0 ||
		strcmp(PropertyName, "Edit Attach Offset") == 0 ||
		strcmp(PropertyName, "AttachBoneName") == 0 ||
		strcmp(PropertyName, "Attach Bone") == 0 ||
		strcmp(PropertyName, "AttachOffsetLocation") == 0 ||
		strcmp(PropertyName, "Attach Offset Location") == 0 ||
		strcmp(PropertyName, "AttachOffsetRotation") == 0 ||
		strcmp(PropertyName, "Attach Offset Rotation") == 0 ||
		strcmp(PropertyName, "AttachOffsetScale") == 0 ||
		strcmp(PropertyName, "Attach Offset Scale") == 0)
	{
		AttachMeshComponent = nullptr;
		if (!bEditAttachOffset)
		{
			UpdateBoneAttachment();
		}
		RebuildCloth(true);
	}
}

void UClothComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	AttachMeshComponent = nullptr;

	if (!ClothMeshPath.empty() && ClothMeshPath != "None")
	{
		ID3D11Device* Device = GEngine
			? GEngine->GetRenderer().GetFD3DDevice().GetDevice()
			: nullptr;

		UStaticMesh* Loaded = FMeshManager::LoadStaticMesh(ClothMeshPath, Device);
		ClothMesh = Loaded;
	}
	else
	{
		ClothMesh = nullptr;
	}

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

	if (ClothMesh)
	{
		FStaticMesh* MeshAsset = ClothMesh->GetStaticMeshAsset();

		if (MeshAsset && !MeshAsset->Vertices.empty() && !MeshAsset->Indices.empty())
		{
			FMeshDataView View;
			View.VertexData = MeshAsset->Vertices.data();
			View.VertexCount = static_cast<uint32>(MeshAsset->Vertices.size());
			View.Stride = sizeof(FNormalVertex);
			View.IndexData = MeshAsset->Indices.data();
			View.IndexCount = static_cast<uint32>(MeshAsset->Indices.size());

			ClothInstance.InitializeMesh(ClothDesc, View);
		}
		else
		{
			ClothInstance.InitializeGrid(ClothDesc);
		}
	}
	else
	{
		ClothInstance.InitializeGrid(ClothDesc);
	}

	UpdateBoneAttachment();

	const FMatrix& WorldMatrix = GetWorldMatrix();
	PreviousSimulationLocation = WorldMatrix.GetLocation();
	PreviousSimulationRotation = WorldMatrix.ToQuat().GetNormalized();
	bHasPreviousSimulationTransform = true;

	ClothInstance.SetSimulationSpaceTransform(PreviousSimulationLocation, PreviousSimulationRotation, true);

	if (bRecreateRenderState)
	{
		MarkRenderStateDirty();
	}

	MarkWorldBoundsDirty();
}

void UClothComponent::SetClothMesh(UStaticMesh* InMesh)
{
	ClothMesh = InMesh;
	ClothMeshPath = ClothMesh ? ClothMesh->GetAssetPathFileName() : "None";

	RebuildCloth(true);
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
