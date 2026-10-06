#pragma once

#include "Distributions/DistributionVector.h"
#include "Math/RandomStream.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

class UStaticMesh;

UENUM()
enum EMeshScreenAlignment : int
{
	PSMA_MeshFaceCameraWithRoll,
	PSMA_MeshFaceCameraWithSpin,
	PSMA_MeshFaceCameraWithLockedAxis,
	PSMA_MAX,
};

UENUM()
enum EMeshCameraFacingUpAxis : int
{
	CameraFacing_NoneUP,
	CameraFacing_ZUp,
	CameraFacing_NegativeZUp,
	CameraFacing_YUp,
	CameraFacing_NegativeYUp,
	CameraFacing_MAX,
};

UENUM()
enum EMeshCameraFacingOptions : int
{
	XAxisFacing_NoUp,
	XAxisFacing_ZUp,
	XAxisFacing_NegativeZUp,
	XAxisFacing_YUp,
	XAxisFacing_NegativeYUp,
	LockedAxis_ZAxisFacing,
	LockedAxis_NegativeZAxisFacing,
	LockedAxis_YAxisFacing,
	LockedAxis_NegativeYAxisFacing,
	VelocityAligned_ZAxisFacing,
	VelocityAligned_NegativeZAxisFacing,
	VelocityAligned_YAxisFacing,
	VelocityAligned_NegativeYAxisFacing,
	EMeshCameraFacingOptions_MAX,
};

UENUM()
enum EParticleAxisLock : int
{
	EPAL_NONE,
	EPAL_X,
	EPAL_Y,
	EPAL_Z,
	EPAL_NEGATIVE_X,
	EPAL_NEGATIVE_Y,
	EPAL_NEGATIVE_Z,
	EPAL_ROTATE_X,
	EPAL_ROTATE_Y,
	EPAL_ROTATE_Z,
	EPAL_MAX,
};

#include "Source/Engine/Particles/TypeData/ParticleModuleTypeDataMesh.generated.h"

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()

	FSoftObjectPtr MeshAssetPath = "None";
	UStaticMesh* Mesh = nullptr;
	FRandomStream RandomStream;
	float LODSizeScale = 1.0f;
	uint8 bUseStaticMeshLODs : 1;
	uint8 CastShadows : 1;
	uint8 DoCollisions : 1;
	EMeshScreenAlignment MeshAlignment = PSMA_MeshFaceCameraWithRoll;
	uint8 bOverrideMaterial : 1;
	uint8 bOverrideDefaultMotionBlurSettings : 1;
	uint8 bEnableMotionBlur : 1;
	FRawDistributionVector RollPitchYawRange;
	EParticleAxisLock AxisLockOption = EPAL_NONE;
	uint8 bCameraFacing : 1;
	EMeshCameraFacingUpAxis CameraFacingUpAxisOption_DEPRECATED = CameraFacing_NoneUP;
	EMeshCameraFacingOptions CameraFacingOption = XAxisFacing_NoUp;
	uint8 bApplyParticleRotationAsSpin : 1;
	uint8 bFaceCameraDirectionRatherThanPosition : 1;
	uint8 bCollisionsConsiderPartilceSize : 1;

	UParticleModuleTypeDataMesh();
	void CreateDistribution();

	// Unreal responsibility:
	// TypeData owns a UStaticMesh asset reference. In this engine, FAssetRegistry
	// exposes the selectable asset path and FMeshManager owns loaded UStaticMesh
	// objects. This function resolves the reference from those asset/cache layers
	// only. It must not create renderer/D3D resources.
	void ResolveMeshFromPath();

	UStaticMesh* GetStaticMesh();

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent& InComponent) override;
	bool IsAMeshEmitter() const override { return true; }
	bool IsMotionBlurEnabled() const;
	void Serialize(FArchive& Ar) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;
};
