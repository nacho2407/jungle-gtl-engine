#pragma once

#include "Object/Object.h"

class UParticleModule : public UObject
{
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
	int32 Level;
	bool bEnabled;

	//UParticleModuleRequired* RequiredModule;
	//UParticleModuleTypeDataBase* TypeDataModule;
	TArray<UParticleModule*> Modules;
};

class UParticleEmitter : public UObject
{
	TArray<UParticleLODLevel*> LODLevels;
};

class UParticleSystem : public UObject
{
	TArray<UParticleEmitter*> Emitters;
};

