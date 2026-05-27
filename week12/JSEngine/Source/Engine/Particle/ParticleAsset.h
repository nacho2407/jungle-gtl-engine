#pragma once

#include "Object/Object.h"
#include "Particle/ParticleModules.h"

/**
 * @brief LOD별 particle runtime cache
 *
 * @details Cascade 스타일 LOD에서는 모든 LOD가 LOD 0의 particle layout을 공유합니다.
 *          각 cache는 현재 LOD에서 실행할 module 목록과, LOD 0 layout 기준 payload offset을 함께 보관합니다.
 */
struct FParticleLODLevelRuntimeCache
{
	int32 ParticleStride = 0;
	int32 PayloadOffset = 0;
	int32 InstancePayloadSize = 0;

	UParticleModuleRequired* RequiredModule = nullptr;
	UParticleModuleSpawn* SpawnModule = nullptr;
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;
	UParticleModuleEventGenerator* EventGeneratorModule = nullptr;

	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
	// 적분 후 이동 구간 query만 수행할 collision module 목록
	TArray<UParticleModuleCollision*> CollisionModules;

	TMap<UParticleModule*, int32> ModulePayloadOffsets;
	TMap<UParticleModule*, int32> ModuleInstanceOffsets;

	int32 GetParticlePayloadOffset(UParticleModule* Module) const;
	int32 GetInstancePayloadOffset(UParticleModule* Module) const;
};

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY(UParticleLODLevel, UObject)
	~UParticleLODLevel() override;
	void PostDuplicate(UObject* Original) override;

	UPROPERTY(DisplayName = "Level")
	int32 Level = 0;
	UPROPERTY(DisplayName = "Enabled")
	bool bEnabled = true;
	UPROPERTY(DisplayName = "Solo")
	bool bSolo = false;

	UPROPERTY(ReferenceType = RuntimeObject)
	UParticleModuleRequired* RequiredModule = nullptr;

	UPROPERTY(ReferenceType = RuntimeObject)
	UParticleModuleSpawn* SpawnModule = nullptr;

	UPROPERTY(ReferenceType = RuntimeObject)
	TArray<UParticleModule*> Modules;

	UPROPERTY(ReferenceType = RuntimeObject)
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;
};

UCLASS()
class UParticleEmitter : public UObject
{
public:
	GENERATED_BODY(UParticleEmitter, UObject)
	~UParticleEmitter() override;
	void PostDuplicate(UObject* Original) override;

	UPROPERTY(ReferenceType = RuntimeObject)
	TArray<UParticleLODLevel*> LODLevels;

	TArray<FParticleLODLevelRuntimeCache> LODLevelRuntimeCaches;

	void CacheEmitterModuleInfo();

	/**
	 * @brief LOD topology가 LOD 0 layout을 공유할 수 있는지 검사합니다.
	 *
	 * @param bLogWarnings warning log 출력 여부
	 *
	 * @return LOD 0 layout 공유 가능 여부
	 *
	 * @details module add/delete는 LOD 0에서만 허용한다는 제약을 런타임에서 검증합니다.
	 *          lower LOD는 LOD 0과 module slot 수, module class, TypeData class가 같아야 합니다.
	 */
	bool ValidateLODTopology(bool bLogWarnings = true) const;

	TArray<int32> CalculateTotalPayloadSize() const;
	FParticleLODLevelRuntimeCache* GetLODLevelRuntimeCache(int32 LODIndex);
	const FParticleLODLevelRuntimeCache* GetLODLevelRuntimeCache(int32 LODIndex) const;
	FParticleLODLevelRuntimeCache* GetLOD0RuntimeCache(); // LOD 0 입니다!
	const FParticleLODLevelRuntimeCache* GetLOD0RuntimeCache() const;
};

UCLASS()
class UParticleSystem : public UObject
{
public:
	GENERATED_BODY(UParticleSystem, UObject)
	UParticleSystem();
	~UParticleSystem() override;
	void PostDuplicate(UObject* Original) override;

	UPROPERTY(ReferenceType = RuntimeObject)
	TArray<UParticleEmitter*> Emitters;

	/**
	 * @brief 거리 기반 LOD 전환 기준 거리 목록
	 *
	 * @details LOD 0은 항상 0.0f로 유지되어야 하며, 각 값은 해당 LOD가 선택되기 시작하는 최소 거리입니다.
	 */
	UPROPERTY(DisplayName = "LOD Distances")
	TArray<float> LODDistances;

	/**
	 * @brief 거리 기반 LOD 전환 히스테리시스 거리
	 *
	 * @details 현재 LOD를 유지하기 위한 전환 여유 거리입니다. 카메라가 LOD 경계 근처에서 흔들릴 때 잦은 LOD 왕복 전환을 줄입니다.
	 */
	UPROPERTY(DisplayName = "LOD Hysteresis Distance", Min = 0.0f, Speed = 10.0f)
	float LODHysteresisDistance = 50.0f;

	void SetAssetPath(const FString& InPath) { AssetPath = InPath; }
	const FString& GetAssetPath() const { return AssetPath; }

private:
	FString AssetPath;
};
