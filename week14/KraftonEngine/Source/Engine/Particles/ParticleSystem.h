#pragma once

#include "Object/Object.h"

struct FParticleEmitterInstance;
class FArchive;
class UParticleModule;
class UParticleSystemComponent;
class UParticleModuleRequired;

#include "Source/Engine/Particles/ParticleSystem.generated.h"

UCLASS()
class UParticleLODLevel : public UObject
{
public:
	GENERATED_BODY()
	~UParticleLODLevel() override;

	int32 GetLevel() const { return Level; }
	bool IsEnabled() const { return bEnabled; }
	const TArray<UParticleModule*>& GetModules() const { return Modules; }
	TArray<UParticleModule*>& GetMutableModules() { return Modules; }

	void SetLevel(int32 InLevel) { Level = InLevel; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	void Serialize(FArchive& Ar) override;

	template<typename T>
	T* FindModule() const
	{
		for (UParticleModule* Module : Modules)
		{
			if (T* TypedModule = dynamic_cast<T*>(Module))
			{
				return TypedModule;
			}
		}
		return nullptr;
	}

private:
	UPROPERTY(Edit, Save, Category="Particle|LOD", DisplayName="Level")
	int32 Level = 0;
	UPROPERTY(Edit, Save, Category="Particle|LOD", DisplayName="Enabled")
	bool bEnabled = true;

	UParticleModuleRequired* RequiredModule;
	//UParticleModuleTypeDataBase* TypeDataModule;
	UPROPERTY(Edit, Save, Instanced, Category="Particle|LOD", DisplayName="Modules", AllowedClass=UParticleModule)
	TArray<UParticleModule*> Modules;
};

class UParticleEmitter : public UObject
{
public:
	~UParticleEmitter() override;

	FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* Component);

	const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }
	TArray<UParticleLODLevel*>& GetMutableLODLevels() { return LODLevels; }
	UParticleLODLevel* GetLODLevel(int32 Index = 0) const;

	int32 GetMaxActiveParticles() const { return MaxActiveParticles; }
	float GetEmitterDuration() const { return EmitterDuration; }
	bool IsLooping() const { return bLooping; }

	void SetMaxActiveParticles(int32 InMaxActiveParticles) { MaxActiveParticles = InMaxActiveParticles; }
	void SetEmitterDuration(float InEmitterDuration) { EmitterDuration = InEmitterDuration; }
	void SetLooping(bool bInLooping) { bLooping = bInLooping; }
	void AddLODLevel(UParticleLODLevel* LODLevel);

	void Serialize(FArchive& Ar) override;

private:
	TArray<UParticleLODLevel*> LODLevels;

	int32 MaxActiveParticles = 100;
	float EmitterDuration = 1.0f;
	bool bLooping = true;
};

class UParticleSystem : public UObject
{
public:
	~UParticleSystem() override;

	void InitializeDefaultEmitters();

	const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }
	TArray<UParticleEmitter*>& GetMutableEmitters() { return Emitters; }
	void AddEmitter(UParticleEmitter* Emitter);

	void SetAssetPathFileName(const FString& InPathFileName) { AssetPathFileName = InPathFileName; }
	const FString& GetAssetPathFileName() const { return AssetPathFileName; }

	void Serialize(FArchive& Ar) override;

private:
	TArray<UParticleEmitter*> Emitters;
	FString AssetPathFileName = "None";
};
