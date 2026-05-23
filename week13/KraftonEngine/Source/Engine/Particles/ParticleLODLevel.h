#pragma once
#include "Object/Object.h"

class UParticleModuleRequired;
class UParticleModule;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;

#include "Source/Engine/Particles/ParticleLODLevel.generated.h"

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY()

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Level")
	int32 Level = 0;

	UPROPERTY(Edit, Save, Category="LOD", DisplayName="Enabled")
	bool bEnabled = true;

	int32 PeakActiveParticles = 0;

	// Required module for this LOD Level
	UPROPERTY(Edit, Save, Category="Modules", DisplayName="Required")
	UParticleModuleRequired* RequiredModule = nullptr;

	UPROPERTY(Edit, Save, Category="Modules", DisplayName="Spawn")
	UParticleModuleSpawn* SpawnModule = nullptr;

	// 해당 LOD 레벨에 대한 Module 들
	TArray<UParticleModule*> Modules;
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
	TArray<UParticleModule*> OrbitModules;

	// 2D 스프라이트 기본형인지, 아니면 3D 메시나 빔, GPU 파티클 같은 특수 확장 형태인지 등의 정보를 담음
	UPROPERTY(Edit, Save, Category="Modules", DisplayName="TypeData")
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	void UpdateModuleLists();
	int32 CalculateMaxActiveParticleCount() const;
};
