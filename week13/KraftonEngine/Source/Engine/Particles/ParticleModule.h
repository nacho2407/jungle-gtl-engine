#pragma once

#include "Object/Object.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Math/Rotator.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleHelper.h"

class UParticleModuleTypeDataBase;
class UMaterial;
struct FParticleEmitterInstance;

enum EParticleSortMode : int32
{
	PSORTMODE_None = 0,
	PSORTMODE_ViewProjDepth,
	PSORTMODE_DistanceToView,
	PSORTMODE_Age_OldestFirst,
	PSORTMODE_Age_NewestFirst
};

struct FParticleSpawnContext
{
    FParticleEmitterInstance& Owner;
    int32 Offset = 0;
    float SpawnTime = 0.0f;
    FBaseParticle* ParticleBase = nullptr;
};

struct FParticleUpdateContext
{
    FParticleEmitterInstance& Owner;
    int32 Offset = 0;
    float DeltaTime = 0.0f;
};

#include "Source/Engine/Particles/ParticleModule.generated.h"

UCLASS()
class UParticleModule : public UObject
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Particle Module", DisplayName="Enabled")
    bool bEnabled = true;

    // Cascade has per-module flags such as bUpdateModule and bFinalUpdateModule.
    // They let the emitter instance call only the modules that participate in each phase.
    UPROPERTY(Edit, Save, Category="Particle Module", DisplayName="Update Module")
    bool bUpdateModule = false;

    UPROPERTY(Edit, Save, Category="Particle Module", DisplayName="Final Update Module")
    bool bFinalUpdateModule = false;

    virtual void Spawn(const FParticleSpawnContext& Context) {}
    virtual void Update(const FParticleUpdateContext& Context) {}
    virtual void PostUpdate(const FParticleUpdateContext& Context) {}
    virtual void FinalUpdate(const FParticleUpdateContext& Context) {}

    virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) { return 0; }
    virtual uint32 RequiredBytesPerInstance() { return 0; }

    virtual bool RequiresRandomSeedInstancePayload() const { return false; }
};

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
    GENERATED_BODY()
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Use Local Space")
    bool bUseLocalSpace = false;

    // Cascade RequiredModule의 EmitterOrigin / EmitterRotation에 해당한다.
    // Emitter local space에서 component space로 들어가기 전의 emitter 자체 오프셋/회전이다.
    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Origin", Type=Vec3)
    FVector EmitterOrigin = FVector::ZeroVector;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Rotation", Type=Rotator)
    FRotator EmitterRotation = FRotator::ZeroRotator;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="SubUV Interpolation")
    EParticleSubUVInterpMethod InterpolationMethod = PSUVIM_None;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Duration", Min=0.001f, Max=100000.0f, Speed=0.1f)
    float EmitterDuration = 1.0f;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Duration Low", Min=-1.0f, Max=100000.0f, Speed=0.1f)
    float EmitterDurationLow = -1.0f;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Delay", Min=0.0f, Max=100000.0f, Speed=0.1f)
    float EmitterDelay = 0.0f;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Delay Low", Min=-1.0f, Max=100000.0f, Speed=0.1f)
    float EmitterDelayLow = -1.0f;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Emitter Loops", Min=0.0f, Max=100000.0f, Speed=1.0f)
    int32 EmitterLoops = 0;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Recalc Duration Each Loop")
    bool bDurationRecalcEachLoop = false;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Delay First Loop Only")
    bool bDelayFirstLoopOnly = false;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Use Legacy Emitter Time")
    bool bUseLegacyEmitterTime = false;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Kill On Deactivate")
    bool bKillOnDeactivate = false;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Kill On Completed")
    bool bKillOnCompleted = false;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Sort Mode")
    int32 SortMode = PSORTMODE_None;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Screen Alignment")
    int32 ScreenAlignment = 0;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Use Max Draw Count")
    bool bUseMaxDrawCount = false;

    UPROPERTY(Edit, Save, Category="Required", DisplayName="Max Draw Count")
    int32 MaxDrawCount = 0;

    UMaterial* Material = nullptr;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Rate", Min=0.0f, Max=100000.0f, Speed=1.0f)
    float Rate = 10.0f;

    UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Rate Scale", Min=0.0f, Max=100000.0f, Speed=0.1f)
    float RateScale = 1.0f;

    UPROPERTY(Edit, Save, Category="Spawn", DisplayName="Burst Scale", Min=0.0f, Max=100000.0f, Speed=0.1f)
    float BurstScale = 1.0f;

    TArray<FParticleBurst> BurstList;

    int32 GetMaximumBurstCount() const;
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Lifetime", DisplayName="Lifetime", Min=0.001f, Max=100000.0f, Speed=0.1f)
    float Lifetime = 1.0f;

    void Spawn(const FParticleSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Location", DisplayName="Start Location", Type=Vec3)
    FVector StartLocation = FVector::ZeroVector;

    void Spawn(const FParticleSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Velocity", DisplayName="Start Velocity", Type=Vec3)
    FVector StartVelocity = FVector::ZeroVector;

    void Spawn(const FParticleSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Color", DisplayName="Start Color", Type=Color4)
    FLinearColor StartColor = FLinearColor::White();

    void Spawn(const FParticleSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Size", DisplayName="Start Size", Type=Vec3)
    FVector StartSize = FVector::OneVector;

    void Spawn(const FParticleSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleSizeScaleBySpeed : public UParticleModule
{
public:
    GENERATED_BODY()

    UParticleModuleSizeScaleBySpeed()
    {
        bUpdateModule = true;
    }

    UPROPERTY(Edit, Save, Category="Size", DisplayName="Speed Scale", Type=Vec3)
    FVector SpeedScale = FVector(1.0f, 1.0f, 1.0f);

    UPROPERTY(Edit, Save, Category="Size", DisplayName="Max Scale", Type=Vec3)
    FVector MaxScale = FVector(10.0f, 10.0f, 10.0f);

    void Update(const FParticleUpdateContext& Context) override;
};

UCLASS()
class UParticleModuleCameraOffset : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Camera Offset", DisplayName="Offset", Min=-100000.0f, Max=100000.0f, Speed=1.0f)
    float Offset = 0.0f;

    uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override { return sizeof(FCameraOffsetParticlePayload); }
    void Spawn(const FParticleSpawnContext& Context) override;
};

enum EOrbitChainMode : uint8
{
    EOChainMode_Add = 0,
    EOChainMode_Scale,
    EOChainMode_Link
};

UCLASS()
class UParticleModuleOrbit : public UParticleModule
{
public:
    GENERATED_BODY()

    UPROPERTY(Edit, Save, Category="Orbit", DisplayName="Chain Mode")
    EOrbitChainMode ChainMode = EOChainMode_Add;

    UPROPERTY(Edit, Save, Category="Orbit", DisplayName="Base Offset", Type=Vec3)
    FVector BaseOffset = FVector::ZeroVector;

    UPROPERTY(Edit, Save, Category="Orbit", DisplayName="Rotation Rate", Type=Vec3)
    FVector RotationRate = FVector::ZeroVector;

    uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override { return sizeof(FOrbitChainModuleInstancePayload); }
    void Spawn(const FParticleSpawnContext& Context) override;
};

UCLASS()
class UParticleModuleLight : public UParticleModule
{
public:
    GENERATED_BODY()
};
