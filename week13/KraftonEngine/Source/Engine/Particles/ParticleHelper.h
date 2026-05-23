#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Vector.h"

#include <cstddef>
#include <random>

#ifndef INDEX_NONE
#define INDEX_NONE -1
#endif

enum EParticleStates : uint32
{
    STATE_Particle_JustSpawned          = 0x02000000,
    STATE_Particle_Freeze               = 0x04000000,
    STATE_Particle_IgnoreCollisions     = 0x08000000,
    STATE_Particle_FreezeTranslation    = 0x10000000,
    STATE_Particle_FreezeRotation       = 0x20000000,
    STATE_Particle_DelayCollisions      = 0x40000000,
    STATE_Particle_CollisionHasOccurred = 0x80000000,

    STATE_Mask                          = 0xFE000000,
    STATE_CounterMask                   = ~STATE_Mask
};

enum EParticleSubUVInterpMethod : uint8
{
    PSUVIM_None = 0,
    PSUVIM_Linear,
    PSUVIM_Linear_Blend,
    PSUVIM_Random,
    PSUVIM_Random_Blend
};

enum EParticleSortMode : int32
{
    PSORTMODE_None = 0,
    PSORTMODE_ViewProjDepth,
    PSORTMODE_DistanceToView,
    PSORTMODE_Age_OldestFirst,
    PSORTMODE_Age_NewestFirst
};

struct FRandomStream
{
    std::mt19937 Generator;

    FRandomStream()
        : Generator(std::random_device{}())
    {
    }

    explicit FRandomStream(uint32 Seed)
        : Generator(Seed)
    {
    }

    void Initialize(uint32 Seed)
    {
        Generator.seed(Seed);
    }

    float FRand()
    {
        std::uniform_real_distribution<float> Dist(0.0f, 1.0f);
        return Dist(Generator);
    }

    int32 RandRange(int32 Min, int32 Max)
    {
        std::uniform_int_distribution<int32> Dist(Min, Max);
        return Dist(Generator);
    }
};

struct FParticleRandomSeedInstancePayload
{
    FRandomStream RandomStream;
};

struct FFullSubUVPayload
{
    float ImageIndex = 0.0f;
    float RandomImageTime = 0.0f;
};

struct FCameraOffsetParticlePayload
{
    float BaseOffset = 0.0f;
    float Offset = 0.0f;
};

struct FOrbitChainModuleInstancePayload
{
    FVector BaseOffset = FVector::ZeroVector;
    FVector Offset = FVector::ZeroVector;
    FVector PreviousOffset = FVector::ZeroVector;

    FVector BaseRotation = FVector::ZeroVector;
    FVector Rotation = FVector::ZeroVector;

    FVector BaseRotationRate = FVector::ZeroVector;
    FVector RotationRate = FVector::ZeroVector;
};

struct FMeshRotationPayloadData
{
    FVector InitialOrientation = FVector::ZeroVector;
    FVector Rotation = FVector::ZeroVector;
    FVector CurContinuousRotation = FVector::ZeroVector;
    FVector RotationRateBase = FVector::ZeroVector;
    FVector RotationRate = FVector::ZeroVector;
};

struct FMeshMotionBlurPayloadData
{
    // Cascade mesh motion blur payload keeps previous-frame values for both base particle
    // fields and mesh-specific payload fields. This is intentionally per-particle payload.
    float BaseParticlePrevRotation = 0.0f;
    FVector BaseParticlePrevVelocity = FVector::ZeroVector;
    FVector BaseParticlePrevSize = FVector::OneVector;
    FVector PayloadPrevRotation = FVector::ZeroVector;
    float PayloadPrevCameraOffset = 0.0f;
    FVector PayloadPrevOrbitOffset = FVector::ZeroVector;
};

struct FParticleEventInstancePayload
{
    bool bSpawnEventsPresent = false;
    bool bDeathEventsPresent = false;
    bool bCollisionEventsPresent = false;

    int32 SpawnEventCount = 0;
    int32 DeathEventCount = 0;
    int32 CollisionEventCount = 0;
};

// FVector is float3 in this engine. Unreal's CPU particle layout relies heavily on
// 16-byte groups. We explicitly add padding fields so key vector blocks begin on
// 16-byte boundaries while keeping the engine-wide FVector layout unchanged.
struct alignas(16) FBaseParticle
{
    FVector OldLocation;
    float OldLocationPadding = 0.0f;

    FVector Location;
    float LocationPadding = 0.0f;

    FVector BaseVelocity;
    float Rotation = 0.0f;

    FVector Velocity;
    float BaseRotationRate = 0.0f;

    FVector BaseSize;
    float RotationRate = 0.0f;

    FVector Size;
    int32 Flags = 0;

    FLinearColor Color;
    FLinearColor BaseColor;

    float RelativeTime = 0.0f;
    float OneOverMaxLifetime = 1.0f;
    float Placeholder0 = 0.0f;
    float Placeholder1 = 0.0f;
};

static_assert(sizeof(FVector) == 12, "FBaseParticle assumes FVector is float3.");
static_assert(sizeof(FLinearColor) == 16, "FBaseParticle assumes FLinearColor is float4.");
static_assert(alignof(FBaseParticle) == 16, "FBaseParticle must be 16-byte aligned.");
static_assert(sizeof(FBaseParticle) % 16 == 0, "FBaseParticle size must be a multiple of 16.");
static_assert(offsetof(FBaseParticle, OldLocation) % 16 == 0);
static_assert(offsetof(FBaseParticle, Location) % 16 == 0);
static_assert(offsetof(FBaseParticle, BaseVelocity) % 16 == 0);
static_assert(offsetof(FBaseParticle, Velocity) % 16 == 0);
static_assert(offsetof(FBaseParticle, BaseSize) % 16 == 0);
static_assert(offsetof(FBaseParticle, Size) % 16 == 0);
static_assert(offsetof(FBaseParticle, Color) % 16 == 0);
static_assert(offsetof(FBaseParticle, BaseColor) % 16 == 0);
static_assert(offsetof(FBaseParticle, RelativeTime) % 16 == 0);

struct FParticleDataContainer
{
    int32 MemBlockSize = 0;
    int32 ParticleDataNumBytes = 0;
    int32 ParticleIndicesNumShorts = 0;

    uint8* ParticleData = nullptr;
    uint16* ParticleIndices = nullptr;

    FParticleDataContainer() = default;
    ~FParticleDataContainer();

    FParticleDataContainer(const FParticleDataContainer&) = delete;
    FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

    FParticleDataContainer(FParticleDataContainer&& Other) noexcept;
    FParticleDataContainer& operator=(FParticleDataContainer&& Other) noexcept;

    void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
    void Free();

    bool IsValid() const
    {
        return ParticleData != nullptr && ParticleIndices != nullptr;
    }
};

#define DECLARE_PARTICLE(Name, Address) \
    FBaseParticle& Name = *reinterpret_cast<FBaseParticle*>(Address)

#define DECLARE_PARTICLE_CONST(Name, Address) \
    const FBaseParticle& Name = *reinterpret_cast<const FBaseParticle*>(Address)

#define DECLARE_PARTICLE_PTR(Name, Address) \
    FBaseParticle* Name = reinterpret_cast<FBaseParticle*>(Address)

#define BEGIN_UPDATE_LOOP                                                     \
{                                                                             \
    int32& ActiveParticles = Context.Owner.ActiveParticles;                    \
    int32 Offset = Context.Offset;                                             \
    uint32 CurrentOffset = static_cast<uint32>(Offset);                        \
    float DeltaTime = Context.DeltaTime;                                       \
    uint8* ParticleData = Context.Owner.ParticleData;                          \
    const uint32 ParticleStride = static_cast<uint32>(Context.Owner.ParticleStride); \
    uint16* ParticleIndices = Context.Owner.ParticleIndices;                   \
    for (int32 i = ActiveParticles - 1; i >= 0; --i)                           \
    {                                                                         \
        const int32 CurrentIndex = ParticleIndices[i];                         \
        uint8* ParticleBase = ParticleData + CurrentIndex * ParticleStride;    \
        FBaseParticle& Particle = *reinterpret_cast<FBaseParticle*>(ParticleBase); \
        if ((Particle.Flags & STATE_Particle_Freeze) == 0)                    \
        {

#define END_UPDATE_LOOP                                                       \
        }                                                                     \
        CurrentOffset = static_cast<uint32>(Offset);                          \
    }                                                                         \
}

#define SPAWN_INIT                                                            \
    const uint32 ParticleStride = static_cast<uint32>(Context.Owner.ParticleStride); \
    uint32 CurrentOffset = static_cast<uint32>(Context.Offset);                \
    FBaseParticle* ParticleBase = Context.ParticleBase;                        \
    FBaseParticle& Particle = *ParticleBase

#define PARTICLE_ELEMENT(Type, Name)                                          \
    Type& Name = *((Type*)((uint8*)ParticleBase + CurrentOffset));																\
	CurrentOffset += sizeof(Type);
