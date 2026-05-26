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

	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;

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

	// 거리 기반 LOD 시스템을 위한 LOD 거리 설정. LOD 0은 항상 0.0f로 설정되어야 함에 유의!
	UPROPERTY(DisplayName = "LOD Distances")
	TArray<float> LODDistances;

	void SetAssetPath(const FString& InPath) { AssetPath = InPath; }
	const FString& GetAssetPath() const { return AssetPath; }

private:
	FString AssetPath;
};
