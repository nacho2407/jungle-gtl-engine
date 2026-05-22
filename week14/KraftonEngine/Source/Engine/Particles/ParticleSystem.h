#pragma once

#include "Object/Object.h"

struct FParticleEmitterInstance;
class UParticleSystemComponent;

class UParticleModule : public UObject
{
public:
	/* 추가해야할 모듈들
	UParticleModuleRequired: Emitter에 필수적인 설정을 포함하며, 파티클의 기본 속성들을 정의합니다.
	UParticleModuleSpawn: 파티클의 생성 빈도와 수량을 제어합니다.
	UParticleModuleLifetime: 파티클의 수명을 설정합니다.
	UParticleModuleLocation: 파티클의 초기 위치를 결정합니다.
	UParticleModuleVelocity: 파티클의 초기 속도와 방향을 설정합니다.
	UParticleModuleColor: 파티클의 색상을 정의하며, 시간에 따른 색상 변화를 설정할 수 있습니다.
	UParticleModuleSize: 파티클의 크기를 설정합니다.
	*/
};

class UParticleLODLevel : public UObject
{
public:
	int32 GetLevel() const { return Level; }
	bool IsEnabled() const { return bEnabled; }
	const TArray<UParticleModule*>& GetModules() const { return Modules; }
	TArray<UParticleModule*>& GetMutableModules() { return Modules; }

	void SetLevel(int32 InLevel) { Level = InLevel; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

private:
	int32 Level = 0;
	bool bEnabled = true;

	//UParticleModuleRequired* RequiredModule;
	//UParticleModuleTypeDataBase* TypeDataModule;
	TArray<UParticleModule*> Modules;
};

class UParticleEmitter : public UObject
{
public:
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

private:
	TArray<UParticleLODLevel*> LODLevels;

	int32 MaxActiveParticles = 100;
	float EmitterDuration = 1.0f;
	bool bLooping = true;
};

class UParticleSystem : public UObject
{
public:
	const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }
	TArray<UParticleEmitter*>& GetMutableEmitters() { return Emitters; }
	void AddEmitter(UParticleEmitter* Emitter);

private:
	TArray<UParticleEmitter*> Emitters;
};
