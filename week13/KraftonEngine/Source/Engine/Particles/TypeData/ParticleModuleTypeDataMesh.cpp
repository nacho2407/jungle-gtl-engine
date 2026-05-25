#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleTypeDataMesh::UParticleModuleTypeDataMesh()
	: bUseStaticMeshLODs(false)
	, CastShadows(false)
	, DoCollisions(false)
	, bOverrideMaterial(false)
	, bOverrideDefaultMotionBlurSettings(false)
	, bEnableMotionBlur(false)
	, bEnableMeshRotation(true)
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
	// UE original responsibility: keep a UStaticMesh asset reference for mesh particles.
	// Missing Jungle foundation: particle type data does not have an asset manager-backed
	// UStaticMesh resolve path yet.
	// System to connect later: engine asset manager StaticMesh load/resolve. TypeData must
	// not create renderer/D3D resources here.
}

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent)
{
	return new FParticleMeshEmitterInstance();
}

bool UParticleModuleTypeDataMesh::IsMotionBlurEnabled() const
{
	return bOverrideDefaultMotionBlurSettings ? bEnableMotionBlur : false;
}

void UParticleModuleTypeDataMesh::Serialize(FArchive& Ar)
{
	UParticleModuleTypeDataBase::Serialize(Ar);
	// UE original responsibility: serialize the UStaticMesh object reference.
	// Missing Jungle foundation: stable UStaticMesh asset reference serializer for TypeData.
	// System to connect later: FSoftObjectPtr-backed StaticMesh asset serialization.
	Ar << LODSizeScale;
	bool UseStaticMeshLODs = bUseStaticMeshLODs;
	bool CastShadowFlag = CastShadows;
	bool DoCollisionFlag = DoCollisions;
	Ar << UseStaticMeshLODs << CastShadowFlag << DoCollisionFlag;
	Ar << reinterpret_cast<int32&>(MeshAlignment);
	bool OverrideMaterial = bOverrideMaterial;
	bool OverrideDefaultMotionBlurSettings = bOverrideDefaultMotionBlurSettings;
	bool EnableMotionBlur = bEnableMotionBlur;
	bool EnableMeshRotation = bEnableMeshRotation;
	Ar << OverrideMaterial << OverrideDefaultMotionBlurSettings << EnableMotionBlur << EnableMeshRotation;
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
		bEnableMeshRotation = EnableMeshRotation;
		bCameraFacing = CameraFacing;
		bApplyParticleRotationAsSpin = ApplyParticleRotationAsSpin;
		bFaceCameraDirectionRatherThanPosition = FaceCameraDirectionRatherThanPosition;
		bCollisionsConsiderPartilceSize = CollisionsConsiderParticleSize;
		ResolveMeshFromPath();
	}
}
