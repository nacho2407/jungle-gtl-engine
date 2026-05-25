#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Object/GarbageCollection.h"

#include "Asset/AssetRegistry.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

namespace
{
	bool IsNoneMeshPath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}

	FString ResolveRegisteredStaticMeshPath(const FString& InPath)
	{
		if (IsNoneMeshPath(InPath))
		{
			return FString();
		}

		const TArray<FAssetListItem>& StaticMeshAssets = FAssetRegistry::ListByTypeName("UStaticMesh");
		for (const FAssetListItem& Item : StaticMeshAssets)
		{
			if (Item.FullPath == InPath || Item.DisplayName == InPath)
			{
				return Item.FullPath;
			}
		}

		// Keep the original path if AssetRegistry has not been refreshed yet.
		// MeshManager will still be queried by package path / cache key below.
		return InPath;
	}

	UStaticMesh* FindCachedStaticMesh(const FString& InPath)
	{
		if (IsNoneMeshPath(InPath))
		{
			return nullptr;
		}

		const FString RegisteredPath = ResolveRegisteredStaticMeshPath(InPath);

		auto It = FMeshManager::StaticMeshCache.find(RegisteredPath);
		if (It != FMeshManager::StaticMeshCache.end())
		{
			return It->second;
		}

		const FString CacheKey = FMeshManager::IsAssetPackagePath(RegisteredPath)
			? RegisteredPath
			: FMeshManager::GetStaticMeshBinaryFilePath(RegisteredPath);

		It = FMeshManager::StaticMeshCache.find(CacheKey);
		if (It != FMeshManager::StaticMeshCache.end())
		{
			return It->second;
		}

		return nullptr;
	}
}

UParticleModuleTypeDataMesh::UParticleModuleTypeDataMesh()
	: bUseStaticMeshLODs(false)
	, CastShadows(false)
	, DoCollisions(false)
	, bOverrideMaterial(false)
	, bOverrideDefaultMotionBlurSettings(false)
	, bEnableMotionBlur(false)
	, bCameraFacing(false)
	, bApplyParticleRotationAsSpin(false)
	, bFaceCameraDirectionRatherThanPosition(false)
	, bCollisionsConsiderPartilceSize(false)
{
	CreateDistribution();
}

void UParticleModuleTypeDataMesh::CreateDistribution()
{
	// UE original responsibility: allocate RollPitchYawRange if missing.
	// Missing Jungle foundation: editor distribution defaults for mesh type data.
	// System to connect later: asset/editor default distribution construction.
}

void UParticleModuleTypeDataMesh::ResolveMeshFromPath()
{
	const FString RequestedPath = MeshAssetPath.ToString();
	Mesh = FindCachedStaticMesh(RequestedPath);

	if (Mesh)
	{
		return;
	}

	// UE original responsibility:
	// TypeData keeps an asset reference and the asset system resolves the
	// UStaticMesh object. Jungle has FAssetRegistry for asset discovery and
	// FMeshManager for cached UStaticMesh objects, but loading a missing mesh
	// still requires an ID3D11Device through FMeshManager::LoadStaticMesh.
	//
	// Keep this as a cache/asset-layer resolve only. Do not call renderer/D3D
	// from TypeData. If the mesh is not already loaded into FMeshManager cache,
	// the renderer/asset preload path must load it before particle dynamic data
	// is built.
}

UStaticMesh* UParticleModuleTypeDataMesh::GetStaticMesh()
{
	if (!Mesh)
	{
		ResolveMeshFromPath();
	}
	return Mesh;
}

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent)
{
	return new FParticleMeshEmitterInstance();
}

bool UParticleModuleTypeDataMesh::IsMotionBlurEnabled() const
{
	return bOverrideDefaultMotionBlurSettings ? bEnableMotionBlur : false;
}

void UParticleModuleTypeDataMesh::AddReferencedObjects(FReferenceCollector& Collector)
{
	UParticleModuleTypeDataBase::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(Mesh, "UParticleModuleTypeDataMesh.Mesh");
	RollPitchYawRange.AddReferencedObjects(Collector);
}

void UParticleModuleTypeDataMesh::Serialize(FArchive& Ar)
{
	UParticleModuleTypeDataBase::Serialize(Ar);

	FString MeshPath = MeshAssetPath.ToString();
	Ar << MeshPath;
	if (Ar.IsLoading())
	{
		MeshAssetPath.SetPath(MeshPath);
	}

	Ar << LODSizeScale;

	bool UseStaticMeshLODs = bUseStaticMeshLODs;
	bool CastShadowFlag = CastShadows;
	bool DoCollisionFlag = DoCollisions;
	Ar << UseStaticMeshLODs << CastShadowFlag << DoCollisionFlag;

	Ar << reinterpret_cast<int32&>(MeshAlignment);

	bool OverrideMaterial = bOverrideMaterial;
	bool OverrideDefaultMotionBlurSettings = bOverrideDefaultMotionBlurSettings;
	bool EnableMotionBlur = bEnableMotionBlur;
	Ar << OverrideMaterial << OverrideDefaultMotionBlurSettings << EnableMotionBlur;

	RollPitchYawRange.Serialize(Ar);
	Ar << reinterpret_cast<int32&>(AxisLockOption);

	bool CameraFacing = bCameraFacing;
	Ar << CameraFacing;

	Ar << reinterpret_cast<int32&>(CameraFacingUpAxisOption_DEPRECATED);
	Ar << reinterpret_cast<int32&>(CameraFacingOption);

	bool ApplyParticleRotationAsSpin = bApplyParticleRotationAsSpin;
	bool FaceCameraDirectionRatherThanPosition = bFaceCameraDirectionRatherThanPosition;
	bool CollisionsConsiderParticleSize = bCollisionsConsiderPartilceSize;
	Ar << ApplyParticleRotationAsSpin << FaceCameraDirectionRatherThanPosition << CollisionsConsiderParticleSize;

	if (Ar.IsLoading())
	{
		bUseStaticMeshLODs = UseStaticMeshLODs;
		CastShadows = CastShadowFlag;
		DoCollisions = DoCollisionFlag;
		bOverrideMaterial = OverrideMaterial;
		bOverrideDefaultMotionBlurSettings = OverrideDefaultMotionBlurSettings;
		bEnableMotionBlur = EnableMotionBlur;
		bCameraFacing = CameraFacing;
		bApplyParticleRotationAsSpin = ApplyParticleRotationAsSpin;
		bFaceCameraDirectionRatherThanPosition = FaceCameraDirectionRatherThanPosition;
		bCollisionsConsiderPartilceSize = CollisionsConsiderParticleSize;
		ResolveMeshFromPath();
	}
}
