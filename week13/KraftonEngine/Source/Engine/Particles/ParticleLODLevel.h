#pragma once
#include "Object/Object.h"

class UParticleModuleRequired;
class UParticleModule;
class UParticleModuleTypeDataBase;

class UParticleLODLevel : public UObject
{
	int32 Level;
	uint32 bEnabled : 1;

	// Required module for this LOD Level
	UParticleModuleRequired* RequiredModule;
	// 해당 LOD 레벨에 대한 Module 들
	TArray<UParticleModule*> Modules;
	// 2D 스프라이트 기본형인지, 아니면 3D 메시나 빔, GPU 파티클 같은 특수 확장 형태인지 등의 정보를 담음
	UParticleModuleTypeDataBase* TypeDataModule;
};
