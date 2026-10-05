#pragma once

#include "Core/CoreMinimal.h"
#include "Particle/ParticleRandom.h"
#include "Particle/ParticleTypes.h"

struct FParticleLODLevelRuntimeCache;
struct FParticleBurstEntry;
class IParticleEmitterInstanceOwner;
class UParticleModule;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModuleTypeDataRibbon;
class UParticleModuleTypeDataAnimTrail;

class FParticleEmitterInstance
{
public:
	FParticleEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: Owner(InOwner)
	{
	}

	virtual ~FParticleEmitterInstance();

	IParticleEmitterInstanceOwner& GetOwner() { return Owner; }
	const IParticleEmitterInstanceOwner& GetOwner() const { return Owner; }

	UParticleEmitter* SpriteTemplate = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;
	const FParticleLODLevelRuntimeCache* CurrentRuntimeCache = nullptr;

	TArray<uint8> ParticleMemoryBlock;
	TArray<uint8> InstanceMemoryBlock;
	FParticleDataContainer DataContainer;

	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	uint8* InstanceData = nullptr;

	int32 ParticleStride = 0;
	int32 PayloadOffset = 0;
	int32 InstancePayloadSize = 0;

	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;
	uint32 ParticleCounter = 0;
	float SpawnFraction = 0.0f;
	float EmitterTime = 0.0f;
	float SecondsSinceCreation = 0.0f;
	int32 CompletedLoopCount = 0;
	bool bEmitterSpawnComplete = false;
	TArray<uint8> BurstFiredThisLoop;
	FParticleRandomStream RandomStream;

	virtual bool Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex);
	virtual void Reset();
	virtual void Release();

	/**
	 * @brief 현재 emitter instance가 참조하는 LOD runtime cache를 교체합니다.
	 *
	 * @param InLODLevelIndex 전환 대상 LOD index
	 *
	 * @return LOD runtime cache 교체 성공 여부
	 *
	 * @details Cascade 스타일 LOD는 particle storage를 재할당하지 않습니다.
	 *          이 함수는 CurrentLODLevelIndex, CurrentLODLevel, CurrentRuntimeCache만 교체합니다.
	 */
	bool SetCurrentLODIndex(int32 InLODLevelIndex);

	int32 GetActiveParticleCount() const { return ActiveParticles; }
	int32 GetLastFrameSpawnedCount() const { return LastFrameSpawnedCount; }
	int32 GetLastFrameKilledCount() const { return LastFrameKilledCount; }

	/**
	 * @brief PSC 안에서 사용하는 emitter 식별 index 저장
	 */
	void SetEmitterIndex(int32 InEmitterIndex) { EmitterIndex = InEmitterIndex; }
	int32 GetEmitterIndex() const { return EmitterIndex; }

	/**
	 * @brief active index에 연결된 고정 storage index 조회
	 * @note event payload의 ParticleIndex에는 이 값을 기록
	 */
	int32 GetPhysicalIndexByActiveIndex(int32 ActiveIndex) const;

	/**
	 * @brief collision 발생 정보를 named event 생성 경로로 전달
	 *
	 * @param Event collision 발생 world space payload
	 */
	void ReportCollisionOccurrence(const FParticleEventPayload& Event);

	/**
	 * @brief 내부 event를 receiver module에 전달
	 */
	void ProcessParticleEvents(const TArray<FParticleEventPayload>& Events);

	/**
	 * @brief event 위치와 속도 조건으로 particle 생성
	 *
	 * @param Event world space 위치와 속도를 가진 내부 payload
	 */
	int32 SpawnParticlesFromEvent(
		const FParticleEventPayload& Event,
		int32 SpawnCount,
		bool bUseParticleSystemLocation,
		bool bInheritVelocity,
		float InheritVelocityScale);

	/**
	 * @brief particle을 pending kill 상태로 표시
	 * @note 실제 storage 제거는 tick 마지막 compact에서 수행
	 */
	bool KillParticleByActiveIndex(int32 ActiveIndex);
	bool IsParticlePendingKill(const FBaseParticle& Particle) const;
	FBaseParticle& GetParticleByActiveIndex(int32 ActiveIndex);
	const FBaseParticle& GetParticleByActiveIndex(int32 ActiveIndex) const;
	FBaseParticle& GetParticleByPhysicalIndex(int32 PhysicalIndex);
	const FBaseParticle& GetParticleByPhysicalIndex(int32 PhysicalIndex) const;
	uint8* GetParticlePayloadByOffset(FBaseParticle& Particle, int32 Offset);
	const uint8* GetParticlePayloadByOffset(const FBaseParticle& Particle, int32 Offset) const;
	uint8* GetParticlePayload(FBaseParticle& Particle, UParticleModule* Module);
	const uint8* GetParticlePayload(const FBaseParticle& Particle, UParticleModule* Module) const;
	uint8* GetModuleInstanceData(UParticleModule* Module);
	const uint8* GetModuleInstanceData(UParticleModule* Module) const;

	/**
	 * @brief 기존 particle을 새 LOD instance에 복사한 뒤 새 LOD의 module payload를 초기화합니다.
	 */
	void InitializeModulePayloads(FBaseParticle& Particle);

	/**
	 * @brief particle의 모든 module payload를 초기화합니다.
	 */
	void ClearParticlePayloads(FBaseParticle& Particle) const;

	bool UsesLocalSpace() const;

	FVector TransformLocationToWorldSpace(const FVector& SimulationLocation) const;
	FVector TransformVelocityToWorldSpace(const FVector& SimulationVelocity) const;
	FVector TransformLocationToSimulationSpace(const FVector& WorldLocation) const;
	FVector TransformVelocityToSimulationSpace(const FVector& WorldVelocity) const;

	float TransformRadiusToWorldSpace(float Radius) const;
	FVector GetParticleLocationForRender(const FBaseParticle& Particle) const;
	void CalculateLocalBounds(FVector& OutMin, FVector& OutMax) const;
	void CalculateWorldBounds(FVector& OutMin, FVector& OutMax) const;

	virtual void Tick(float DeltaTime);

protected:
	bool AllocateParticleData(int32 InMaxActiveParticles, int32 InParticleStride, int32 InInstancePayloadSize);
	bool RefreshCurrentRuntimeCache();
	const UParticleModuleRequired* GetRequiredModule() const;
	float GetEmitterDuration() const;
	int32 GetTotalLoopCount() const;
	bool IsInfiniteLooping() const;
	bool CanSpawnEmitter() const;
	void ResetLoopRuntimeState();
	void ResetBurstFiredState();
	int32 GetCurrentLODMaxParticles() const;
	void CompleteEmitterLoop();
	void TickEmitterSpawn(float DeltaTime);
	void TickEmitterSpawnSegment(float SegmentStartTime, float SegmentEndTime);
	int32 CalculateSpawnRateCount(float DeltaTime);
	int32 CalculateBurstSpawnCount(float PreviousEmitterTime, float CurrentEmitterTime);
	int32 ResolveBurstSpawnAmount(const FParticleBurstEntry& Entry);
	virtual int32 SpawnParticles(int32 Count, float SegmentStartTime, float SegmentDeltaTime);

	/**
	 * @brief trail source 위치에 개별 particle 생성
	 *
	 * @param WorldLocation particle 중심 world 위치
	 * @param SpawnSide trail 폭 방향 world 벡터
	 */
	virtual int32 SpawnParticle(const FVector& WorldLocation, const FVector& SpawnSide, float SpawnTime);

	/**
	 * @brief 내부 event payload로 개별 particle 생성
	 *
	 * @note normal spawn event를 다시 생성하지 않는 receiver 전용 경로
	 */
	int32 SpawnParticleFromEvent(
		const FParticleEventPayload& Event,
		bool bUseParticleSystemLocation,
		bool bInheritVelocity,
		float InheritVelocityScale);

	/**
	 * @brief normal spawn named event 생성
	 */
	void GenerateSpawnEvent(const FBaseParticle& Particle, int32 PhysicalIndex);
	/**
	 * @brief 최초 death named event 생성
	 */
	void GenerateDeathEvent(const FBaseParticle& Particle, int32 PhysicalIndex);
	/**
	 * @brief collision named event 생성
	 */
	void GenerateCollisionEvent(const FParticleEventPayload& Event);
	void MarkParticlePendingKill(int32 ActiveIndex);
	void CompactPendingKilledParticles();
	void AgeParticles(float DeltaTime);
	void UpdateModules(float DeltaTime);
	void IntegrateParticles(float DeltaTime);

	void UpdateCollisionModules(float DeltaTime);

	IParticleEmitterInstanceOwner& Owner;
	int32 EmitterIndex = -1;
	int32 LastFrameSpawnedCount = 0;
	int32 LastFrameKilledCount = 0;
};

class FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleMeshEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleEmitterInstance(InOwner)
	{
	}
};

class FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleBeamEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleEmitterInstance(InOwner)
	{
	}
};

class FParticleTrailsEmitterInstance : public FParticleEmitterInstance
{
public:
	explicit FParticleTrailsEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleEmitterInstance(InOwner)
	{
	}
};

struct FRibbonTrailRuntimeState
{
	FVector LastSourcePosition = FVector::ZeroVector;
	float DistanceRemainder = 0.0f;
	bool bHasSource = false;
};

class FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance
{
public:
	explicit FParticleRibbonEmitterInstance(IParticleEmitterInstanceOwner& InOwner) : FParticleTrailsEmitterInstance(InOwner) {}

	bool Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex) override;
	void Reset() override;
	void Tick(float DeltaTime) override;

	void BuildRenderSnapshot(TArray<FRibbonRenderPoint>& OutPoints, TArray<FRibbonRenderRange>& OutRanges) const;
	int32 GetRibbonPointCount() const;

private:
	UParticleModuleTypeDataRibbon* RibbonTypeData = nullptr;
	TArray<FRibbonTrailRuntimeState> Trails;
	FVector PendingSpawnSourceStart = FVector::ZeroVector;
	FVector PendingSpawnSourceEnd = FVector::ZeroVector;
	FVector PendingSpawnSourceSide = FVector::RightVector;
	bool bHasPendingSpawnSource = false;

	int32 SpawnParticles(int32 Count, float SegmentStartTime, float SegmentDeltaTime) override;

	void UpdateRibbonTrail(float DeltaTime);
	bool HasEnabledSpawnModule() const;
	void PrepareSpawnModuleSourceSpan();
	void FinishSpawnModuleSourceSpan();
	FVector GetRibbonSourcePosition() const;
	FVector GetRibbonSourceSide() const;
};


class FParticleAnimTrailEmitterInstance : public FParticleTrailsEmitterInstance
{
public:
	explicit FParticleAnimTrailEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleTrailsEmitterInstance(InOwner)
	{
	}

	bool Init(UParticleEmitter* InTemplate, int32 InLODLevelIndex) override;
	void Reset() override;

	void BeginTrail();
	void EndTrail();
	void SetTrailSourceData(const FString& InFirstSocketName, const FString& InSecondSocketName, float InWidth);

private:
	UParticleModuleTypeDataAnimTrail* AnimTrailTypeData = nullptr;
	FString FirstSocketName;
	FString SecondSocketName;
	float TrailWidth = 1.0f;
	bool bTrailEnabled = false;
};
