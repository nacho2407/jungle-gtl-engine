#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/RandomStream.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#ifndef INDEX_NONE
#define INDEX_NONE -1
#endif

enum EParticleStates : uint32
{
	STATE_Particle_JustSpawned = 0x02000000,
	STATE_Particle_Freeze = 0x04000000,
	STATE_Particle_IgnoreCollisions = 0x08000000,
	STATE_Particle_FreezeTranslation = 0x10000000,
	STATE_Particle_FreezeRotation = 0x20000000,
	STATE_Particle_DelayCollisions = 0x40000000,
	STATE_Particle_CollisionHasOccurred = 0x80000000,

	STATE_Mask = 0xFE000000,
	STATE_CounterMask = ~STATE_Mask
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

	FVector Rotation = FVector::ZeroVector;

	FVector BaseRotationRate = FVector::ZeroVector;

	FVector RotationRate = FVector::ZeroVector;

	FVector PreviousOffset = FVector::ZeroVector;
};

struct FMeshRotationPayloadData
{
	FVector InitialOrientation = FVector::ZeroVector;
	FVector  InitRotation = FVector::ZeroVector;
	FVector Rotation = FVector::ZeroVector;
	FVector CurContinuousRotation = FVector::ZeroVector;
	FVector RotationRate = FVector::ZeroVector;
	FVector RotationRateBase = FVector::ZeroVector;
};

struct FMeshMotionBlurPayloadData
{
	// Cascade mesh motion blur payload keeps previous-frame values for both base particle
	// fields and mesh-specific payload fields. This is intentionally per-particle payload.
	FVector BaseParticlePrevVelocity = FVector::ZeroVector;
	FVector BaseParticlePrevSize = FVector::OneVector;
	FVector PayloadPrevRotation = FVector::ZeroVector;
	FVector PayloadPrevOrbitOffset = FVector::ZeroVector;
	float BaseParticlePrevRotation = 0.0f;
	float PayloadPrevCameraOffset = 0.0f;
};

//
// TypeDataBeam2 payload
//
#define BEAM2_TYPEDATA_LOCKED_MASK					0x80000000
#define BEAM2_TYPEDATA_LOCKED(x)					(((x) & BEAM2_TYPEDATA_LOCKED_MASK) != 0)
#define BEAM2_TYPEDATA_SETLOCKED(x, Locked)			((x) = (Locked) ? ((x) | BEAM2_TYPEDATA_LOCKED_MASK) : ((x) & ~BEAM2_TYPEDATA_LOCKED_MASK))

#define BEAM2_TYPEDATA_FREQUENCY_MASK				0x00fff000
#define BEAM2_TYPEDATA_FREQUENCY_SHIFT				12
#define BEAM2_TYPEDATA_FREQUENCY(x)					(((x) & BEAM2_TYPEDATA_FREQUENCY_MASK) >> BEAM2_TYPEDATA_FREQUENCY_SHIFT)
#define BEAM2_TYPEDATA_SETFREQUENCY(x, Freq)		((x) = (((x) & ~BEAM2_TYPEDATA_FREQUENCY_MASK) | ((Freq) << BEAM2_TYPEDATA_FREQUENCY_SHIFT)))

struct FBeam2TypeDataPayload
{
	FVector SourcePoint = FVector::ZeroVector;
	FVector SourceTangent = FVector::XAxisVector;
	float SourceStrength = 0.0f;

	FVector TargetPoint = FVector::ZeroVector;
	FVector TargetTangent = FVector::XAxisVector;
	float TargetStrength = 0.0f;

	int32 Lock_Max_NumNoisePoints = 0;
	int32 InterpolationSteps = 0;
	FVector Direction = FVector::XAxisVector;
	double StepSize = 0.0;
	int32 Steps = 0;
	float TravelRatio = 0.0f;
	int32 TriangleCount = 0;
	int32 Flags = 0;
};

struct FBeamParticleSourceTargetPayloadData
{
	int32 ParticleIndex = INDEX_NONE;
};

struct FBeamParticleSourceBranchPayloadData
{
	int32 NoiseIndex = INDEX_NONE;
};

struct FBeamParticleModifierPayloadData
{
	uint32 bModifyPosition : 1;
	uint32 bScalePosition : 1;
	uint32 bModifyTangent : 1;
	uint32 bScaleTangent : 1;
	uint32 bModifyStrength : 1;
	uint32 bScaleStrength : 1;
	FVector Position = FVector::ZeroVector;
	FVector Tangent = FVector::ZeroVector;
	float Strength = 0.0f;

	FBeamParticleModifierPayloadData()
		: bModifyPosition(false)
		, bScalePosition(false)
		, bModifyTangent(false)
		, bScaleTangent(false)
		, bModifyStrength(false)
		, bScaleStrength(false)
	{
	}

	void UpdatePosition(FVector& Value)
	{
		if (bModifyPosition)
		{
			Value = bScalePosition ? (Value * Position) : (Value + Position);
		}
	}

	void UpdateTangent(FVector& Value, bool bAbsolute)
	{
		if (bModifyTangent)
		{
			FVector ModTangent = Tangent;
			if (!bAbsolute)
			{
				const FVector From = FVector::XAxisVector;
				const FVector To = Value.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
				float Dot = From.Dot(To);
				Dot = Dot < -1.0f ? -1.0f : (Dot > 1.0f ? 1.0f : Dot);
				if (Dot < -0.9999f)
				{
					ModTangent = FQuat::FromAxisAngle(FVector::YAxisVector, 3.14159265358979323846f).RotateVector(Tangent);
				}
				else if (Dot < 0.9999f)
				{
					const FVector Axis = From.Cross(To).GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
					ModTangent = FQuat::FromAxisAngle(Axis, std::acos(Dot)).RotateVector(Tangent);
				}
			}
			Value = bScaleTangent ? (Value * ModTangent) : (Value + ModTangent);
		}
	}

	void UpdateStrength(float& Value)
	{
		if (bModifyStrength)
		{
			Value = bScaleStrength ? (Value * Strength) : (Value + Strength);
		}
	}
};

#define TRAIL_EMITTER_FLAG_FORCEKILL	0x00000000
#define TRAIL_EMITTER_FLAG_DEADTRAIL	0x10000000
#define TRAIL_EMITTER_FLAG_MIDDLE		0x20000000
#define TRAIL_EMITTER_FLAG_START		0x40000000
#define TRAIL_EMITTER_FLAG_END			0x80000000
#define TRAIL_EMITTER_FLAG_MASK			0xf0000000
#define TRAIL_EMITTER_PREV_MASK			0x0fffc000
#define TRAIL_EMITTER_PREV_SHIFT		14
#define TRAIL_EMITTER_NEXT_MASK			0x00003fff
#define TRAIL_EMITTER_NEXT_SHIFT		0

#define TRAIL_EMITTER_NULL_PREV			(TRAIL_EMITTER_PREV_MASK >> TRAIL_EMITTER_PREV_SHIFT)
#define TRAIL_EMITTER_NULL_NEXT			(TRAIL_EMITTER_NEXT_MASK >> TRAIL_EMITTER_NEXT_SHIFT)

#define TRAIL_EMITTER_CHECK_FLAG(val, mask, flag)				(((val) & (mask)) == (flag))
#define TRAIL_EMITTER_SET_FLAG(val, mask, flag)					(((val) & ~(mask)) | (flag))
#define TRAIL_EMITTER_GET_PREVNEXT(val, mask, shift)			(((val) & (mask)) >> (shift))
#define TRAIL_EMITTER_SET_PREVNEXT(val, mask, shift, setval)	(((val) & ~(mask)) | (((setval) << (shift)) & (mask)))

#define TRAIL_EMITTER_IS_START(index)		TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_START)
#define TRAIL_EMITTER_SET_START(index)		TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_START)
#define TRAIL_EMITTER_IS_END(index)			TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_END)
#define TRAIL_EMITTER_SET_END(index)		TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_END)
#define TRAIL_EMITTER_IS_MIDDLE(index)		TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_MIDDLE)
#define TRAIL_EMITTER_SET_MIDDLE(index)		TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_MIDDLE)
#define TRAIL_EMITTER_IS_ONLY(index)		(TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_START) && (TRAIL_EMITTER_GET_NEXT(index) == TRAIL_EMITTER_NULL_NEXT))
#define TRAIL_EMITTER_SET_ONLY(index)		TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_START)
#define TRAIL_EMITTER_IS_FORCEKILL(index)	TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_FORCEKILL)
#define TRAIL_EMITTER_SET_FORCEKILL(index)	TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_FORCEKILL)
#define TRAIL_EMITTER_IS_DEADTRAIL(index)	TRAIL_EMITTER_CHECK_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_DEADTRAIL)
#define TRAIL_EMITTER_SET_DEADTRAIL(index)	TRAIL_EMITTER_SET_FLAG(index, TRAIL_EMITTER_FLAG_MASK, TRAIL_EMITTER_FLAG_DEADTRAIL)
#define TRAIL_EMITTER_IS_HEAD(index)		(TRAIL_EMITTER_IS_START(index) || TRAIL_EMITTER_IS_DEADTRAIL(index))
#define TRAIL_EMITTER_IS_HEADONLY(index)	((TRAIL_EMITTER_IS_START(index) || TRAIL_EMITTER_IS_DEADTRAIL(index)) && (TRAIL_EMITTER_GET_NEXT(index) == TRAIL_EMITTER_NULL_NEXT))
#define TRAIL_EMITTER_GET_PREV(index)		TRAIL_EMITTER_GET_PREVNEXT(index, TRAIL_EMITTER_PREV_MASK, TRAIL_EMITTER_PREV_SHIFT)
#define TRAIL_EMITTER_SET_PREV(index, prev)	TRAIL_EMITTER_SET_PREVNEXT(index, TRAIL_EMITTER_PREV_MASK, TRAIL_EMITTER_PREV_SHIFT, prev)
#define TRAIL_EMITTER_GET_NEXT(index)		TRAIL_EMITTER_GET_PREVNEXT(index, TRAIL_EMITTER_NEXT_MASK, TRAIL_EMITTER_NEXT_SHIFT)
#define TRAIL_EMITTER_SET_NEXT(index, next)	TRAIL_EMITTER_SET_PREVNEXT(index, TRAIL_EMITTER_NEXT_MASK, TRAIL_EMITTER_NEXT_SHIFT, next)

#define TRAIL_EMITTER_FORCEKILL		TRAIL_EMITTER_FLAG_FORCEKILL
#define TRAIL_EMITTER_DEADTRAIL		TRAIL_EMITTER_FLAG_DEADTRAIL
#define TRAIL_EMITTER_MIDDLE		TRAIL_EMITTER_FLAG_MIDDLE
#define TRAIL_EMITTER_START			TRAIL_EMITTER_FLAG_START
#define TRAIL_EMITTER_END			TRAIL_EMITTER_FLAG_END
#define TRAIL_EMITTER_ONLY			TRAIL_EMITTER_SET_NEXT(TRAIL_EMITTER_SET_PREV(TRAIL_EMITTER_SET_ONLY(0), TRAIL_EMITTER_NULL_PREV), TRAIL_EMITTER_NULL_NEXT)

struct FTrailsBaseTypeDataPayload
{
	int32 Flags = TRAIL_EMITTER_ONLY;
	int32 TrailIndex = INDEX_NONE;
	int32 TriangleCount = 0;
	float SpawnTime = 0.0f;
	float SpawnDelta = 0.0f;
	float TiledU = 0.0f;
	int32 SpawnedTessellationPoints = 0;
	int32 RenderingInterpCount = 1;
	float PinchScaleFactor = 1.0f;
	uint32 bInterpolatedSpawn : 1;
	uint32 bMovementSpawned : 1;

	FTrailsBaseTypeDataPayload()
		: bInterpolatedSpawn(false)
		, bMovementSpawned(false)
	{
	}
};

struct FRibbonTypeDataPayload : public FTrailsBaseTypeDataPayload
{
	FVector Tangent = FVector::XAxisVector;
	FVector Up = FVector::ZAxisVector;
	int32 SourceIndex = INDEX_NONE;
};

struct FParticleEventInstancePayload
{
	bool bSpawnEventsPresent = false;
	bool bDeathEventsPresent = false;
	bool bCollisionEventsPresent = false;
	bool  bBurstEventsPresent = false;

	int32 SpawnTrackingCount = 0;
	int32 DeathTrackingCount = 0;
	int32 CollisionTrackingCount = 0;
	int32 BurstTrackingCount = 0;
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

