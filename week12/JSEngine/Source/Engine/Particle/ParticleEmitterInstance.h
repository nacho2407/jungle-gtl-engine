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
	 * @param Event 기존 외부 collision payload와 동일한 발생 정보
	 */
	void ReportCollisionOccurrence(const FParticleEventCollideData& Event);

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

private:
	bool AllocateParticleData(int32 InMaxActiveParticles, int32 InParticleStride, int32 InInstancePayloadSize);
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
	int32 SpawnParticles(
		int32 Count,
		float SegmentStartTime,
		float SegmentDeltaTime);
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
	void GenerateCollisionEvent(const FParticleEventCollideData& Event);
	void MarkParticlePendingKill(int32 ActiveIndex);
	void CompactPendingKilledParticles();
	void AgeParticles(float DeltaTime);
	void UpdateModules(float DeltaTime);
	void IntegrateParticles(float DeltaTime);

	void UpdateCollisionModules(float DeltaTime);

	IParticleEmitterInstanceOwner& Owner;
	int32 EmitterIndex = -1;
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

class FParticleRibbonEmitterInstance : public FParticleTrailsEmitterInstance
{
public:
	explicit FParticleRibbonEmitterInstance(IParticleEmitterInstanceOwner& InOwner)
		: FParticleTrailsEmitterInstance(InOwner)
	{
	}
};
