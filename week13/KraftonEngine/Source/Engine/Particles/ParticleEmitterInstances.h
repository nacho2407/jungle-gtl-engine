#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"

#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/DynamicEmitterData.h"
#include "Particles/ParticleModule.h"

class UParticleSystemComponent;
class UMaterial;

struct FLODBurstFired
{
	TArray<bool> Fired;
};

struct FParticleEmitterInstance
{
    static constexpr float PeakActiveParticleUpdateDelta = 0.05f;

    UParticleEmitter* SpriteTemplate = nullptr;
    UParticleSystemComponent* Component = nullptr;

    UParticleLODLevel* CurrentLODLevel = nullptr;
    int32 CurrentLODLevelIndex = 0;

    int32 TypeDataOffset = 0;
    int32 TypeDataInstanceOffset = -1;
    int32 SubUVDataOffset = 0;
    int32 DynamicParameterDataOffset = 0;
    int32 LightDataOffset = 0;
    int32 OrbitModuleOffset = 0;
    int32 CameraPayloadOffset = 0;
    int32 PayloadOffset = 0;

    FVector Location = FVector::ZeroVector;
    FVector OldLocation = FVector::ZeroVector;
    FVector PositionOffsetThisTick = FVector::ZeroVector;
    FVector PivotOffset = FVector::ZeroVector;

    // Cascade 원본의 EmitterToSimulation / SimulationToWorld.
    // bUseLocalSpace=false: EmitterToSimulation = EmitterToComponent * ComponentToWorldNoScale, SimulationToWorld = Identity
    // bUseLocalSpace=true : EmitterToSimulation = EmitterToComponent, SimulationToWorld = ComponentToWorldNoScale
    FMatrix EmitterToSimulation = FMatrix::Identity;
    FMatrix SimulationToWorld = FMatrix::Identity;

    bool bEnabled = true;
    bool bKillOnDeactivate = false;
    bool bKillOnCompleted = false;
    bool bRequiresSorting = false;
    bool bHaltSpawning = false;
    bool bHaltSpawningExternal = false;
    bool bRequiresLoopNotification = false;
    bool bIgnoreComponentScale = false;
    bool bIsBeam = false;
    bool bAxisLockEnabled = false;
    bool bFakeBurstsWhenSpawningSupressed = false;
    bool bEmitterIsDone = false;

    int32 SortMode = 0;

    uint8* ParticleData = nullptr;
    uint16* ParticleIndices = nullptr;
    uint8* InstanceData = nullptr;

    int32 InstancePayloadSize = 0;
    int32 ParticleSize = sizeof(FBaseParticle);
    int32 ParticleStride = sizeof(FBaseParticle);

    int32 ActiveParticles = 0;
    uint32 ParticleCounter = 0;
    int32 MaxActiveParticles = 0;
    int32 PeakActiveParticles = 0;

    float SpawnFraction = 0.0f;
    float SecondsSinceCreation = 0.0f;
    float EmitterTime = 0.0f;
    float LastDeltaTime = 0.0f;

    FBoundingBox ParticleBoundingBox;

    TArray<FLODBurstFired> BurstFired;

    int32 LoopCount = 0;
    int32 IsRenderDataDirty = 0;

    float EmitterDuration = 0.0f;
    TArray<float> EmitterDurations;
    float CurrentDelay = 0.0f;

    int32 TrianglesToRender = 0;
    int32 MaxVertexIndex = 0;

    int32 EventCount = 0;
    int32 MaxEventCount = 0;

    UMaterial* CurrentMaterial = nullptr;

    FParticleEmitterInstance() = default;
    virtual ~FParticleEmitterInstance();

    virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
    virtual void Init();
    virtual void FreeResources();

    virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true);

    virtual void Tick(float DeltaTime, bool bSuppressSpawning);

    // Component transform / RequiredModule EmitterOrigin, EmitterRotation을 반영해
    // emitter local -> simulation, simulation -> world 변환을 갱신한다.
    virtual void UpdateTransforms();
    virtual void ApplyWorldOffset(FVector InOffset, bool bWorldShift);

    virtual float Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
    virtual float Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime);
    virtual void Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
    virtual void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);
    virtual void Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel);

    virtual void CheckEmitterFinished();
    virtual void FakeBursts();

    virtual void SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess);
    virtual void Rewind();

    virtual FBoundingBox GetBoundingBox() const;
    virtual void UpdateBoundingBox(float DeltaTime);
    virtual void ForceUpdateBoundingBox();

    virtual uint32 RequiredBytes();
    virtual uint32 CalculateParticleStride(uint32 InParticleSize);

    virtual void SetupEmitterDuration();

    virtual void ResetBurstList();
    virtual float GetCurrentBurstRateOffset(float& DeltaTime, int32& Burst);

    virtual void ResetParticleParameters(float DeltaTime);
    virtual void CalculateOrbitOffset(
        FOrbitChainModuleInstancePayload& Payload,
        FVector& AccumOffset,
        FVector& AccumRotation,
        FVector& AccumRotationRate,
        float DeltaTime,
        FVector& Result,
        FMatrix& RotationMat);
    virtual void UpdateOrbitData(float DeltaTime);
    virtual void ParticlePrefetch();

    virtual float Spawn(float DeltaTime);

    void Spawn(float OldLeftover, float Rate, float DeltaTime, int32 Burst, float BurstTime);

    void SpawnParticles(
        int32 Count,
        float StartTime,
        float Increment,
        const FVector& InitialLocation,
        const FVector& InitialVelocity,
        FParticleEventInstancePayload* EventPayload = nullptr);

    virtual void ForceSpawn(
        float DeltaTime,
        int32 InSpawnCount,
        int32 InBurstCount,
        FVector& InLocation,
        FVector& InVelocity);

    virtual void CheckSpawnCount(int32 InNewCount, int32 InMaxCount);

    virtual void PreSpawn(
        FBaseParticle* Particle,
        const FVector& InitialLocation,
        const FVector& InitialVelocity);

    virtual void PostSpawn(
        FBaseParticle* Particle,
        float InterpolationPercentage,
        float SpawnTime);

    virtual bool HasCompleted() const;

    virtual void KillParticles();
    virtual void KillParticle(int32 Index);
    virtual void KillParticlesForced(bool bFireEvents = false);

    virtual void FixupParticleIndices();

    virtual int32 GetOrbitPayloadOffset();
    virtual FVector GetParticleLocationWithOrbitOffset(FBaseParticle* Particle);

    virtual FBaseParticle* GetParticle(int32 Index);
    virtual FBaseParticle* GetParticleDirect(int32 DirectIndex);

    uint32 GetModuleDataOffset(UParticleModule* Module) const;
    uint8* GetModuleInstanceData(UParticleModule* Module) const;
    FParticleRandomSeedInstancePayload* GetModuleRandomSeedInstanceData(UParticleModule* Module) const;
    virtual uint8* GetTypeDataModuleInstanceData() const;

    UParticleLODLevel* GetCurrentLODLevelChecked() const;

    virtual bool IsDynamicDataRequired() const;
    virtual bool FillReplayData(FDynamicEmitterReplayDataBase& OutData);
    virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected);
    virtual void ProcessParticleEvents(float DeltaTime, bool bSuppressSpawning);
    virtual void Tick_MaterialOverrides(int32 EmitterIndex);
    virtual bool UseLocalSpace();
    virtual void GetScreenAlignmentAndScale(int32& OutScreenAlign, FVector& OutScale);
    virtual UMaterial* GetCurrentMaterial();

    FVector GetParticleBaseSize(const FBaseParticle& Particle) const
    {
        return Particle.BaseSize;
    }
};

struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
    FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
    bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};

struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
    int32 MeshRotationOffset = 0;
    int32 MeshMotionBlurOffset = 0;

    bool bMeshRotationActive = true;
    bool bMotionBlurEnabled = false;

    uint32 RequiredBytes() override;
    void Tick(float DeltaTime, bool bSuppressSpawning) override;
    void UpdateBoundingBox(float DeltaTime) override;

    void PreSpawn(
        FBaseParticle* Particle,
        const FVector& InitialLocation,
        const FVector& InitialVelocity) override;

    void PostSpawn(
        FBaseParticle* Particle,
        float InterpolationPercentage,
        float SpawnTime) override;

    FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
    bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};
