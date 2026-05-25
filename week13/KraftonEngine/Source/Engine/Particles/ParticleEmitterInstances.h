#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"

#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/DynamicEmitterData.h"
#include "Particles/ParticleModule.h"

class UParticleSystemComponent;
class UMaterial;
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataBeam2;
class UParticleModuleTypeDataRibbon;
class UParticleModuleBeamSource;
class UParticleModuleBeamTarget;
class UParticleModuleBeamNoise;
class UParticleModuleBeamModifier;
class UParticleModuleTrailSource;
class UParticleModuleSpawnPerUnit;

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
	bool bUseParticlePrefetch = false;

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
    UParticleModuleTypeDataMesh* MeshTypeData = nullptr;
    int32 MeshRotationOffset = 0;
    int32 MeshMotionBlurOffset = 0;

    bool bMeshRotationActive = true;
    bool bMotionBlurEnabled = false;
    TArray<UMaterial*> CurrentMaterials;

    void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;
    void Init() override;
    bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true) override;
    uint32 RequiredBytes() override;
    void Tick(float DeltaTime, bool bSuppressSpawning) override;
    void UpdateBoundingBox(float DeltaTime) override;

    void PostSpawn(
        FBaseParticle* Particle,
        float InterpolationPercentage,
        float SpawnTime) override;

    void Tick_MaterialOverrides(int32 EmitterIndex) override;
    void SetMeshMaterials(const TArray<UMaterial*>& InMaterials);
    void GetMeshMaterials(TArray<UMaterial*>& OutMaterials, const UParticleLODLevel* LODLevel, bool bLogWarnings = false) const;
    FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
    bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};

struct FParticleBeam2EmitterInstance : public FParticleEmitterInstance
{
    UParticleModuleTypeDataBeam2* BeamTypeData = nullptr;
    UParticleModuleBeamSource* BeamModule_Source = nullptr;
    UParticleModuleBeamTarget* BeamModule_Target = nullptr;
    UParticleModuleBeamNoise* BeamModule_Noise = nullptr;
    UParticleModuleBeamModifier* BeamModule_SourceModifier = nullptr;
    int32 BeamModule_SourceModifier_Offset = INDEX_NONE;
    UParticleModuleBeamModifier* BeamModule_TargetModifier = nullptr;
    int32 BeamModule_TargetModifier_Offset = INDEX_NONE;

    bool FirstEmission = true;
    int32 TickCount = 0;
    int32 ForceSpawnCount = 0;
    int32 BeamMethod = 0;
    TArray<int32> TextureTiles;
    int32 BeamCount = 0;
    void* SourceActor = nullptr;
    FParticleEmitterInstance* SourceEmitter = nullptr;
    TArray<FVector> UserSetSourceArray;
    TArray<FVector> UserSetSourceTangentArray;
    TArray<float> UserSetSourceStrengthArray;
    TArray<float> DistanceArray;
    TArray<FVector> TargetPointArray;
    TArray<FVector> TargetTangentArray;
    TArray<float> UserSetTargetStrengthArray;
    void* TargetActor = nullptr;
    FParticleEmitterInstance* TargetEmitter = nullptr;
    TArray<FName> TargetPointSourceNames;
    TArray<FVector> UserSetTargetArray;
    TArray<FVector> UserSetTargetTangentArray;
    int32 VertexCount = 0;
    int32 TriangleCount = 0;
    TArray<int32> BeamTrianglesPerSheet;

    void SetBeamEndPoint(FVector NewEndPoint);
    void SetBeamSourcePoint(FVector NewSourcePoint, int32 SourceIndex);
    void SetBeamSourceTangent(FVector NewTangentPoint, int32 SourceIndex);
    void SetBeamSourceStrength(float NewSourceStrength, int32 SourceIndex);
    void SetBeamTargetPoint(FVector NewTargetPoint, int32 TargetIndex);
    void SetBeamTargetTangent(FVector NewTangentPoint, int32 TargetIndex);
    void SetBeamTargetStrength(float NewTargetStrength, int32 TargetIndex);
    void ApplyWorldOffset(FVector InOffset, bool bWorldShift) override;

    bool GetBeamEndPoint(FVector& OutEndPoint) const;
    bool GetBeamSourcePoint(int32 SourceIndex, FVector& OutSourcePoint) const;
    bool GetBeamSourceTangent(int32 SourceIndex, FVector& OutTangentPoint) const;
    bool GetBeamSourceStrength(int32 SourceIndex, float& OutSourceStrength) const;
    bool GetBeamTargetPoint(int32 TargetIndex, FVector& OutTargetPoint) const;
    bool GetBeamTargetTangent(int32 TargetIndex, FVector& OutTangentPoint) const;
    bool GetBeamTargetStrength(int32 TargetIndex, float& OutTargetStrength) const;

    void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;
    void Init() override;
    void Tick(float DeltaTime, bool bSuppressSpawning) override;
    void Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* CurrentLODLevel) override;
    void SetCurrentLODIndex(int32 InLODIndex, bool bInFullyProcess) override;
    void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime) override;
    void UpdateBoundingBox(float DeltaTime) override;
    void ForceUpdateBoundingBox() override;
    uint32 RequiredBytes() override;
    float SpawnBeamParticles(float OldLeftover, float Rate, float DeltaTime, int32 Burst = 0, float BurstTime = 0.0f);
    void KillParticles() override;
    void SetupBeamModifierModulesOffsets();
    void ResolveSource();
    void ResolveTarget();
    void DetermineVertexAndTriangleCount();
    FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
    UMaterial* GetCurrentMaterial() override;
    bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};

struct FParticleTrailsEmitterInstance_Base : public FParticleEmitterInstance
{
    int32 VertexCount = 0;
    int32 TriangleCount = 0;
    int32 TrailCount = 0;
    int32 MaxTrailCount = 0;
    float RunningTime = 0.0f;
    float LastTickTime = 0.0f;
    uint32 bDeadTrailsOnDeactivate : 1;
    TArray<float> TrailSpawnTimes;
    TArray<float> LastSpawnTime;
    TArray<float> SourceDistanceTraveled;
    TArray<float> TiledUDistanceTraveled;
    uint32 bFirstUpdate : 1;
    uint32 bEnableInactiveTimeTracking : 1;
    int32 CurrentStartIndices[128];
    int32 CurrentEndIndices[128];

    FParticleTrailsEmitterInstance_Base();
    void Init() override;
    void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;
    void Tick(float DeltaTime, bool bSuppressSpawning) override;
    bool AddParticleHelper(int32 InTrailIdx, int32 StartParticleIndex, FTrailsBaseTypeDataPayload* StartTrailData, int32 ParticleIndex, FTrailsBaseTypeDataPayload* TrailData);
    virtual void Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel);
    void UpdateBoundingBox(float DeltaTime) override;
    void ForceUpdateBoundingBox() override;
    void KillParticles() override;
    virtual void KillParticles(int32 InTrailIdx, int32 InKillCount);
    virtual void SetupTrailModules() {}
    virtual void UpdateSourceData(float DeltaTime, bool bFirstTime);

    void SetStartIndex(int32 TrailIndex, int32 ParticleIndex);
    void SetEndIndex(int32 TrailIndex, int32 ParticleIndex);
    void SetDeadIndex(int32 TrailIndex, int32 ParticleIndex);
    void ClearIndices(int32 TrailIndex, int32 ParticleIndex);

    template<typename TrailDataType>
    void GetTrailStart(const int32 TrailIdx, int32& OutStartIndex, TrailDataType*& OutTrailData, FBaseParticle*& OutParticle)
    {
        OutStartIndex = INDEX_NONE;
        OutTrailData = nullptr;
        OutParticle = nullptr;
        if (TrailIdx != INDEX_NONE && CurrentStartIndices[TrailIdx] != INDEX_NONE)
        {
            OutStartIndex = CurrentStartIndices[TrailIdx];
            OutParticle = GetParticleDirect(OutStartIndex);
            OutTrailData = OutParticle ? reinterpret_cast<TrailDataType*>(reinterpret_cast<uint8*>(OutParticle) + TypeDataOffset) : nullptr;
        }
    }

    template<typename TrailDataType>
    void GetTrailEnd(const int32 TrailIdx, int32& OutEndIndex, TrailDataType*& OutTrailData, FBaseParticle*& OutParticle)
    {
        OutEndIndex = INDEX_NONE;
        OutTrailData = nullptr;
        OutParticle = nullptr;
        if (TrailIdx != INDEX_NONE && CurrentEndIndices[TrailIdx] != INDEX_NONE)
        {
            OutEndIndex = CurrentEndIndices[TrailIdx];
            OutParticle = GetParticleDirect(OutEndIndex);
            OutTrailData = OutParticle ? reinterpret_cast<TrailDataType*>(reinterpret_cast<uint8*>(OutParticle) + TypeDataOffset) : nullptr;
        }
    }

protected:
    enum EGetTrailDirection { GET_Prev, GET_Next };
    enum EGetTrailParticleOption { GET_Any, GET_Spawned, GET_Interpolated, GET_Start, GET_End };
    bool GetParticleInTrail(bool bSkipStartingParticle, FBaseParticle* InStartingFromParticle, FTrailsBaseTypeDataPayload* InStartingTrailData, EGetTrailDirection InGetDirection, EGetTrailParticleOption InGetOption, FBaseParticle*& OutParticle, FTrailsBaseTypeDataPayload*& OutTrailData);
};

struct FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance_Base
{
    UParticleModuleTypeDataRibbon* TrailTypeData = nullptr;
    UParticleModuleSpawnPerUnit* SpawnPerUnitModule = nullptr;
    UParticleModuleTrailSource* SourceModule = nullptr;
    int32 TrailModule_Source_Offset = INDEX_NONE;
    TArray<FVector> CurrentSourcePosition;
    TArray<FQuat> CurrentSourceRotation;
    TArray<FVector> CurrentSourceUp;
    TArray<FVector> CurrentSourceTangent;
    TArray<float> CurrentSourceTangentStrength;
    TArray<FVector> LastSourcePosition;
    TArray<FQuat> LastSourceRotation;
    TArray<FVector> LastSourceUp;
    TArray<FVector> LastSourceTangent;
    TArray<float> LastSourceTangentStrength;
    void* SourceActor = nullptr;
    TArray<FVector> SourceOffsets;
    FParticleEmitterInstance* SourceEmitter = nullptr;
    int32 LastSelectedParticleIndex = INDEX_NONE;
    TArray<int32> SourceIndices;
    TArray<float> SourceTimes;
    TArray<float> LastSourceTimes;
    TArray<float> CurrentLifetimes;
    TArray<float> CurrentSizes;
    int32 HeadOnlyParticles = 0;

    void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent) override;
    void Tick_RecalculateTangents(float DeltaTime, UParticleLODLevel* CurrentLODLevel) override;
    bool GetSpawnPerUnitAmount(float DeltaTime, int32 InTrailIdx, int32& OutCount, float& OutRate);
    void GetParticleLifetimeAndSize(int32 InTrailIdx, const FBaseParticle* InParticle, bool bInNoLivingParticles, float& OutOneOverMaxLifetime, float& OutSize);
    float Spawn(float DeltaTime) override;
    bool Spawn_Source(float DeltaTime);
    float Spawn_RateAndBurst(float DeltaTime);
    void SetupTrailModules() override;
    void ResolveSource();
    void UpdateSourceData(float DeltaTime, bool bFirstTime) override;
    bool ResolveSourcePoint(int32 InTrailIdx, FVector& OutPosition, FQuat& OutRotation, FVector& OutUp, FVector& OutTangent, float& OutTangentStrength);
    void DetermineVertexAndTriangleCount();
    bool IsDynamicDataRequired() const override;
    FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
    void ApplyWorldOffset(FVector InOffset, bool bWorldShift) override;
    bool FillReplayData(FDynamicEmitterReplayDataBase& OutData) override;
};
