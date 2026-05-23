#include "Particles/ParticleModule.h"
#include "Particles/ParticleEmitterInstances.h"

#include <algorithm>

static FVector ParticleComponentMax(const FVector& A, const FVector& B)
{
    return FVector(std::max(A.X, B.X), std::max(A.Y, B.Y), std::max(A.Z, B.Z));
}

static FVector ParticleComponentMin(const FVector& A, const FVector& B)
{
    return FVector(std::min(A.X, B.X), std::min(A.Y, B.Y), std::min(A.Z, B.Z));
}

int32 UParticleModuleSpawn::GetMaximumBurstCount() const
{
    int32 Result = 0;
    for (const FParticleBurst& Burst : BurstList)
    {
        Result += std::max(0, Burst.Count);
    }
    return Result;
}

void UParticleModuleLifetime::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    const float SafeLifetime = std::max(0.001f, Lifetime);
    Particle.RelativeTime = 0.0f;
    Particle.OneOverMaxLifetime = 1.0f / SafeLifetime;
}

void UParticleModuleLocation::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    Particle.Location += StartLocation;
    Particle.OldLocation = Particle.Location;
}

void UParticleModuleVelocity::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    Particle.BaseVelocity = StartVelocity;
    Particle.Velocity = StartVelocity;
}

void UParticleModuleColor::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    Particle.BaseColor = StartColor;
    Particle.Color = StartColor;
}

void UParticleModuleSize::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    Particle.BaseSize = StartSize;
    Particle.Size = StartSize;
}

void UParticleModuleSizeScaleBySpeed::Update(const FParticleUpdateContext& Context)
{
    BEGIN_UPDATE_LOOP;
        const float Speed = Particle.Velocity.Length();
        FVector Size = SpeedScale * Speed;
        Size = ParticleComponentMax(Size, FVector::OneVector);
        Size = ParticleComponentMin(Size, MaxScale);
        Particle.Size = FVector(
            Particle.BaseSize.X * Size.X,
            Particle.BaseSize.Y * Size.Y,
            Particle.BaseSize.Z * Size.Z);
    END_UPDATE_LOOP;
}

void UParticleModuleCameraOffset::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    FCameraOffsetParticlePayload& Payload =
        *reinterpret_cast<FCameraOffsetParticlePayload*>(
            reinterpret_cast<uint8*>(ParticleBase) + CurrentOffset);
    Payload.BaseOffset = Offset;
    Payload.Offset = Offset;
}

void UParticleModuleOrbit::Spawn(const FParticleSpawnContext& Context)
{
    SPAWN_INIT;
    FOrbitChainModuleInstancePayload& Payload =
        *reinterpret_cast<FOrbitChainModuleInstancePayload*>(
            reinterpret_cast<uint8*>(ParticleBase) + CurrentOffset);
    Payload.BaseOffset = BaseOffset;
    Payload.Offset = BaseOffset;
    Payload.Rotation = FVector::ZeroVector;
    Payload.PreviousOffset = BaseOffset;
    Payload.BaseRotationRate = RotationRate;
    Payload.RotationRate = RotationRate;
}
