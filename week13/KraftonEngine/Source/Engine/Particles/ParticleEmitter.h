#pragma once

#include "Object/Object.h"
#include "Object/FName.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"

class FArchive;
class UParticleModule;
class UParticleModuleCameraOffset;
class UParticleModuleColor;
class UParticleModuleLifetime;
class UParticleModuleLocation;
class UParticleModuleOrbit;
class UParticleModuleRequired;
class UParticleModuleSize;
class UParticleModuleSizeScaleBySpeed;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;
class UParticleModuleVelocity;

//enum EParticleSubUVInterpMethod : uint8
//{
//	PSUVIM_None = 0,
//	PSUVIM_Linear,
//	PSUVIM_Linear_Blend,
//	PSUVIM_Random,
//	PSUVIM_Random_Blend
//};

#include "Source/Engine/Particles/ParticleEmitter.generated.h"

USTRUCT()
struct FParticleBurst
{
	GENERATED_BODY()

	int32 Count = 0;

	/** If >= 0, use as a range [CountLow..Count] */
	int32 CountLow = -1;

	/** The time at which to burst them (0..1: emitter lifetime) */
	float Time = 0.0f;
};

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY()

	UParticleEmitter()           = default;
	~UParticleEmitter() override = default;

	void CacheEmitterModuleInfo();
	void Serialize(FArchive& Ar) override;

	void InitializeDefaultSpriteEmitter();
	bool HasValidLOD0() const;
	
	TArray<UParticleLODLevel*>&       GetLODLevels() { return LODLevels; }
	const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }

	UParticleLODLevel* GetLODLevel(int32 LODIndex) const;

	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	float GetQualityLevelSpawnRateMult() const { return QualityLevelSpawnRateScale; }

public:
	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Emitter Name")
	FName EmitterName;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Use Mesh Instance")
	bool bUseMeshInstance = false;

	TArray<UParticleLODLevel*> LODLevels;

	TMap<UParticleModule*, uint32> ModuleOffsetMap;
	TMap<UParticleModule*, uint32> ModuleInstanceOffsetMap;
	TMap<UParticleModule*, uint32> ModuleRandomSeedInstanceOffsetMap;

	int32 ParticleSize = sizeof(FBaseParticle);
	int32 ReqInstanceBytes = 0;

	int32 TypeDataOffset = 0;
	int32 TypeDataInstanceOffset = -1;

	int32 DynamicParameterDataOffset = 0;
	int32 LightDataOffset = 0;
	int32 CameraPayloadOffset = 0;
	int32 OrbitModuleOffset = 0;

	bool bUseLegacySpawningBehavior = false;
	bool bMeshRotationActive = false;

	int32 InitialAllocationCount = 0;
	float QualityLevelSpawnRateScale = 1.0f;
	FVector PivotOffset = FVector::ZeroVector;

private:
	void AddModuleOffsetToAllLODs(int32 ModuleIndex, uint32 Offset);
	void AddModuleInstanceOffsetToAllLODs(int32 ModuleIndex, uint32 Offset);
	void AddModuleRandomSeedOffsetToAllLODs(int32 ModuleIndex, uint32 Offset);

private:
	bool bEnabled = true;
};
