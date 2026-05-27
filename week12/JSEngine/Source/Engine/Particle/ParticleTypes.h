#pragma once

#include "Core/CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
class UMaterialInterface;
class UStaticMesh;
class UTexture;
class UParticleModuleRequired;

UENUM()
enum class EParticleSortMode
{
	None UMETA(DisplayName = "None"),
	ViewDepthBackToFront UMETA(DisplayName = "View Depth Back To Front"),
	ViewDepthFrontToBack UMETA(DisplayName = "View Depth Front To Back"),
	RelativeTime UMETA(DisplayName = "Relative Time"),
};

UENUM()
enum class EParticleCoordinateSpace
{
	Local UMETA(DisplayName = "Local"),
	World UMETA(DisplayName = "World"),
};

UENUM()
enum class EParticleRibbonFacingMode
{
	Billboard UMETA(DisplayName = "Billboard"),
	SourceTransform UMETA(DisplayName = "Keep Source Transform"),
};

enum class EDynamicEmitterType
{
	Sprite,
	Mesh,
	Beam,
	Ribbon,
};

UENUM()
enum class EParticleFlags : uint32
{
	None                 = 0u,
	PendingKill          = 1u << 0,
	IgnoreCollisions     = 1u << 2,
	Freeze               = 1u << 3,
	FreezeTranslation    = 1u << 4,
	FreezeRotation       = 1u << 5,
	FreezeMovement       = 1u << 6,
};

struct FParticleEventCollideData
{
	int32 EmitterIndex = -1;
	int32 ParticleIndex = -1; // physical index
	uint32 SpawnId = 0;

	float ParticleTime = 0.0f;
	float CollisionTime = 1.0f;

	FVector Location = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;

	int FaceIndex = -1;

	AActor* HitActor = nullptr;
	UPrimitiveComponent* HitComponent = nullptr;
};

struct FRibbonParticlePayload
{
	FVector SpawnSide = FVector::RightVector;
};

struct FBaseParticle
{
	FVector Location = FVector::ZeroVector;
	FVector OldLocation = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector BaseVelocity = FVector::ZeroVector;
	// RelativeTime은 수명 기준 0~1 값
	// DelayAmount는 초 단위라서 AgeSeconds와 비교
	float RelativeTime = 0.0f;
	float AgeSeconds = 0.0f;
	float Lifetime = 1.0f;
	float OneOverMaxLifetime = 1.0f; // particle이 현재 수명의 몇 퍼센트 지점에 있는지 계산할 때 사용하는 MaxLifetime의 역수
	float Rotation = 0.0f;
	float RotationRate = 0.0f;
	FVector MeshRotation = FVector::ZeroVector;
	FVector Size = FVector::OneVector;
	FVector BaseSize = FVector::OneVector;
	FColor Color = FColor::White();
	FColor BaseColor = FColor::White();
	uint32 Flags = 0;
	uint32 Seed = 0; // seeded procedural 재현성을 위한 랜덤 시드값

	/**
	 * @brief ribbon emitter 내 정렬이나 LOD preserve 시 MaxParticles가 줄어들면 어떤 Particle을 먼저 죽일지
	 *        결정하기 위한 particle 생성 순서 보존 식별자
	 */
	uint32 SpawnId = 0;
};

inline bool HasParticleFlag(const FBaseParticle& Particle, EParticleFlags Flag)
{
	return (Particle.Flags & static_cast<uint32>(Flag)) != 0u;
}

inline void SetParticleFlag(FBaseParticle& Particle, EParticleFlags Flag)
{
	Particle.Flags |= static_cast<uint32>(Flag);
}

inline void ClearParticleFlag(FBaseParticle& Particle, EParticleFlags Flag)
{
	Particle.Flags &= ~static_cast<uint32>(Flag);
}

struct FParticleSpriteVertex
{
	FVector Position;
	FVector2 UV;
	FVector4 Color;
};

struct FMeshParticleInstanceVertex
{
	FMatrix Transform = FMatrix::Identity;
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct FParticleDataContainer
{
	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;

	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
};

struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType EmitterType = EDynamicEmitterType::Sprite;
	int32 ActiveParticleCount = 0;
	// Simulation memory stride. This is not the renderer vertex stride.
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;

	EParticleCoordinateSpace CoordinateSpace = EParticleCoordinateSpace::Local;
	FVector Scale = FVector::OneVector;
	EParticleSortMode SortMode = EParticleSortMode::ViewDepthBackToFront;

	/**
	 * @brief render replay snapshot의 active index에 해당하는 particle을 조회합니다.
	 *
	 * @param ActiveIndex snapshot-local active particle index
	 *
	 * @return 조회된 particle 포인터 또는 유효하지 않으면 nullptr
	 *
	 * @details render snapshot은 원본 emitter storage와 다른 compacted index 배열을 가질 수 있으므로,
	 *          active index와 physical index 양쪽의 snapshot buffer 범위를 모두 확인합니다.
	 */
	const FBaseParticle* GetParticleByActiveIndex(int32 ActiveIndex) const
	{
		if (ActiveIndex < 0 || ActiveIndex >= ActiveParticleCount)
		{
			return nullptr;
		}

		if (DataContainer.ParticleData == nullptr || DataContainer.ParticleIndices == nullptr || ParticleStride <= 0)
		{
			return nullptr;
		}

		if (ActiveIndex >= DataContainer.ParticleIndicesNumShorts)
		{
			return nullptr;
		}

		const uint16 PhysicalIndex = DataContainer.ParticleIndices[ActiveIndex];
		const size_t ParticleOffset = static_cast<size_t>(PhysicalIndex) * static_cast<size_t>(ParticleStride);
		if (ParticleOffset + sizeof(FBaseParticle) > static_cast<size_t>(DataContainer.ParticleDataNumBytes))
		{
			return nullptr;
		}

		return reinterpret_cast<const FBaseParticle*>(DataContainer.ParticleData + ParticleOffset);
	}
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicSpriteEmitterReplayDataBase() { EmitterType = EDynamicEmitterType::Sprite; }

	UParticleModuleRequired* RequiredModule = nullptr;
	int32 SubUVPayloadOffset = -1;
	int32 SubUVColumns = 1;
	int32 SubUVRows = 1;
	UTexture* SubUVTexture = nullptr;
};

struct FDynamicMeshEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicMeshEmitterReplayDataBase() { EmitterType = EDynamicEmitterType::Mesh; }
};

/**
 * @brief Beam emitter render replay snapshot 데이터
 */
struct FDynamicBeamEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	FDynamicBeamEmitterReplayDataBase() { EmitterType = EDynamicEmitterType::Beam; }

	// Beam 시작점. CoordinateSpace가 Local이면 component local space, World면 world space
	FVector SourcePoint = FVector::ZeroVector;

	// Beam 끝점. CoordinateSpace가 Local이면 component local space, World면 world space
	FVector TargetPoint = FVector(100.0f, 0.0f, 0.0f);

	// Beam quad 두께의 기본 폭
	float BeamWidth = 10.0f;
};

struct FRibbonRenderPoint
{
	// Ribbon 중심선의 한 점입니다. 렌더 프록시가 인접 점 두 개를 segment instance로 전개합니다.
	FVector Position = FVector::ZeroVector;
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	float Width = 1.0f;
	float U = 0.0f;
	FVector Side = FVector::RightVector;
};

struct FRibbonRenderRange
{
	int32 PointStart = 0;
	int32 PointCount = 0;
};

struct FDynamicTrailEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	int32 TrailCount = 0;
	int32 RenderPointCount = 0;
	int32 SheetsPerTrail = 1;
	float TilingDistance = 100.0f;
	EParticleRibbonFacingMode RibbonFacingMode = EParticleRibbonFacingMode::Billboard;
};

struct FDynamicRibbonEmitterReplayDataBase : public FDynamicTrailEmitterReplayDataBase
{
	FDynamicRibbonEmitterReplayDataBase() { EmitterType = EDynamicEmitterType::Ribbon; }

	// Ribbon은 particle snapshot 대신 중심선 point snapshot을 직접 보유합니다.
	TArray<FRibbonRenderPoint> RenderPoints;
	TArray<FRibbonRenderRange> TrailRanges;
};

struct FDynamicEmitterDataBase
{
	virtual ~FDynamicEmitterDataBase() = default;

	int32 EmitterIndex = -1;
	UMaterialInterface* Material = nullptr;
	FMatrix ComponentToWorld = FMatrix::Identity;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	EDynamicEmitterType GetEmitterType() const { return GetSource().EmitterType; }
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase ReplayData;
	TArray<uint8> OwnedParticleData;
	TArray<uint16> OwnedParticleIndices;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicMeshEmitterData : public FDynamicEmitterDataBase
{
	FDynamicMeshEmitterReplayDataBase ReplayData;

	// StaticMesh asset들은 ResourceManager가 소유
	// renderer는 TypeData를 다시 조회하지 않고 이 snapshot의 Mesh 참조를 소비
	UStaticMesh* Mesh = nullptr;

	// ReplayData.DataContainer가 참조하는 현재 프레임 Mesh particle snapshot 소유 버퍼
	TArray<uint8> OwnedParticleData;
	TArray<uint16> OwnedParticleIndices;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

/**
 * @brief Beam emitter render thread 전달 데이터
 */
struct FDynamicBeamEmitterData : public FDynamicEmitterDataBase
{
	FDynamicBeamEmitterReplayDataBase ReplayData;

	// ReplayData.DataContainer가 참조하는 현재 frame Beam particle snapshot 소유 buffer
	TArray<uint8> OwnedParticleData;
	TArray<uint16> OwnedParticleIndices;

	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicTrailsEmitterData : public FDynamicEmitterDataBase
{
};

struct FDynamicAnimTrailsEmitterData : public FDynamicTrailsEmitterData
{
	FDynamicTrailEmitterReplayDataBase ReplayData;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};

struct FDynamicRibbonEmitterData : public FDynamicTrailsEmitterData
{
	FDynamicRibbonEmitterReplayDataBase ReplayData;
	const FDynamicEmitterReplayDataBase& GetSource() const override { return ReplayData; }
};
