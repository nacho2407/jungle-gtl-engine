#pragma once
#include "Object/Object.h"
#include "Object/Ptr/ObjectPtr.h"

class UParticleModuleRequired;
class UParticleModule;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;
class UParticleModuleEventGenerator;

#include "Source/Engine/Particles/ParticleLODLevel.generated.h"

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY()

	// Owned particle module selected from Modules. Serialized explicitly by Serialize().
	UPROPERTY(Transient, Instanced, Category = Event)
	TObjectPtr<UParticleModuleEventGenerator> EventGenerator = nullptr;

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Level")
	int32 Level = 0;

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Enabled")
	bool bEnabled = true;

	int32 PeakActiveParticles = 0;

	// Required module for this LOD Level. Serialized explicitly as class + payload.
	UPROPERTY(Transient, Instanced, Category="Modules", DisplayName="Required")
	TObjectPtr<UParticleModuleRequired> RequiredModule = nullptr;

	UPROPERTY(Transient, Instanced, Category="Modules", DisplayName="Spawn")
	TObjectPtr<UParticleModuleSpawn> SpawnModule = nullptr;

	// 해당 LOD 레벨이 소유하는 generic modules. Serialized explicitly by Serialize().
	UPROPERTY(Transient, Instanced, Category="Modules", DisplayName="Modules")
	TArray<TObjectPtr<UParticleModule>> Modules;
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
	TArray<UParticleModule*> OrbitModules;

	// 2D 스프라이트 기본형인지, 아니면 3D 메시나 빔, GPU 파티클 같은 특수 확장 형태인지 등의 정보를 담음
	UPROPERTY(Transient, Instanced, Category="Modules", DisplayName="TypeData")
	TObjectPtr<UParticleModuleTypeDataBase> TypeDataModule = nullptr;

	void UpdateModuleLists();
	int32 CalculateMaxActiveParticleCount() const;

	void Serialize(FArchive& Ar) override;

    void AddReferencedObjects(FReferenceCollector& Collector) override;
};
