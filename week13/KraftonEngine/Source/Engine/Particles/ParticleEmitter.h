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

	UPROPERTY(EditAnywhere, Category="Burst", DisplayName="Count")
	int32 Count = 0;

	/** If >= 0, use as a range [CountLow..Count] */
	UPROPERTY(EditAnywhere, Category="Burst", DisplayName="Count Low")
	int32 CountLow = -1;

	/** The time at which to burst them (0..1: emitter lifetime) */
	UPROPERTY(EditAnywhere, Category="Burst", DisplayName="Time", Min="0.0", Max="1.0", Speed="0.01")
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

	UFUNCTION(Pure, Category="Particle")
	bool IsEnabled() const { return bEnabled; }
	UFUNCTION(Callable, Exec, Category="Particle")
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	UFUNCTION(Pure, Category="Particle")
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

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Use Legacy Spawning Behavior")
	bool bUseLegacySpawningBehavior = false;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Initial Allocation Count", Min="0")
	int32 InitialAllocationCount = 0;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Quality Level Spawn Rate Scale", Min="0.0", Speed="0.05")
	float QualityLevelSpawnRateScale = 1.0f;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Pivot Offset")
	FVector PivotOffset = FVector::ZeroVector;

    void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
	void AddModuleOffsetToAllLODs(int32 ModuleIndex, uint32 Offset);
	void AddModuleInstanceOffsetToAllLODs(int32 ModuleIndex, uint32 Offset);
	void AddModuleRandomSeedOffsetToAllLODs(int32 ModuleIndex, uint32 Offset);

private:
	bool bEnabled = true;
};
