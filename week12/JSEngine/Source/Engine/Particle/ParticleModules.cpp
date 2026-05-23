#include "Particle/ParticleModules.h"

#include "Particle/ParticleAsset.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Particle/ParticleEmitterInstanceOwner.h"

namespace
{
	// 사용되지 않는 선언 컴파일 경고 방지용. 구현 후 삭제할 것
	void ParticleNoOp(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
	{
		(void)Owner;
		(void)Offset;
		(void)DeltaTime;
	}
}

void UParticleModuleRequired::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleRequired::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSpawn::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSpawn::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLifetime::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleLocation::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleVelocity::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleColor::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleSize::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleTypeDataBase::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleTypeDataBase::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	ParticleNoOp(Owner, Offset, DeltaTime);
}

void UParticleModuleTypeDataBase::Build()
{
}

FParticleEmitterInstance* UParticleModuleTypeDataBase::CreateInstance(
	UParticleEmitter* InEmitterTemplate,
	IParticleEmitterInstanceOwner& InOwner)
{
	FParticleEmitterInstance* Instance = new FParticleEmitterInstance(InOwner);
	Instance->SpriteTemplate = InEmitterTemplate;
	return Instance;
}

FDynamicEmitterDataBase* UParticleModuleTypeDataBase::GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance)
{
	(void)InEmitterInstance;
	return nullptr;
}

int32 UParticleModuleTypeDataBase::GetRequiredPayloadSize() const
{
	return 0;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ParticleSize = CalculateTotalPayloadSize();
}

TArray<int32> UParticleEmitter::CalculateTotalPayloadSize() const
{
	TArray<int32> Result;
	Result.reserve(LODLevels.size());

	for (const UParticleLODLevel* LODLevel : LODLevels)
	{
		int32 PayloadSize = 0;
		if (LODLevel != nullptr && LODLevel->TypeDataModule != nullptr)
		{
			PayloadSize += LODLevel->TypeDataModule->GetRequiredPayloadSize();
		}

		Result.push_back(static_cast<int32>(sizeof(FBaseParticle)) + PayloadSize);
	}

	return Result;
}
