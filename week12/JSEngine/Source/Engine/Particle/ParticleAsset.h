#pragma once

#include "Object/Object.h"
#include "Particle/ParticleModules.h"

/**
 * @brief 매 프레임 업데이트 루프에서 자주 참조되는 정보를 캐싱하여, 런타임에 빠르게 접근할 수 있도록 하는 구조체.
 *		  모듈마다 매번 payload offset를 계산하지 않아도 되도록 합니다.
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

	UPROPERTY()
	int32 Level = 0;
	UPROPERTY()
	bool bEnabled = true;

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
	
	UPROPERTY(ReferenceType = RuntimeObject)
	TArray<UParticleLODLevel*> LODLevels;

	TArray<FParticleLODLevelRuntimeCache> LODLevelRuntimeCaches;

	void CacheEmitterModuleInfo();
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
	~UParticleSystem() override;
	
	UPROPERTY(ReferenceType = RuntimeObject)
	TArray<UParticleEmitter*> Emitters;

	void SetAssetPath(const FString& InPath) { AssetPath = InPath; }
	const FString& GetAssetPath() const { return AssetPath; }

private:
	FString AssetPath;
};
