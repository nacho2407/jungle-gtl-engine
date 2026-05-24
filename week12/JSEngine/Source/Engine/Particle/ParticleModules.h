#pragma once

#include "Object/Object.h"
#include "Particle/ParticleDistributions.h"
#include "Particle/ParticleTypes.h"
#include "Render/Resource/Material.h"

class FParticleEmitterInstance;
class IParticleEmitterInstanceOwner;
class UParticleEmitter;
class UParticleModuleTypeDataBase;
class UStaticMesh;

UCLASS(Abstract)
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule, UObject)

	virtual int32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const;
	virtual int32 RequiredBytesPerInstance(UParticleModuleTypeDataBase* TypeData) const;
	virtual bool IsSpawnRateModule() const;
	virtual bool IsSpawnModule() const;
	virtual bool IsUpdateModule() const;
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime);
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime);
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleRequired, UParticleModule)

	UPROPERTY(DisplayName = "Sort Mode")
	EParticleSortMode SortMode = EParticleSortMode::ViewDepthBackToFront;

	UPROPERTY(DisplayName = "Coordinate Space")
	EParticleCoordinateSpace CoordinateSpace = EParticleCoordinateSpace::Local;

	UPROPERTY(DisplayName = "Max Particles", Min = 0.0f, Max = 65535.0f, Speed = 1.0f)
	int32 MaxParticles = 1000;

	UPROPERTY(DisplayName = "Emitter Duration", Min = 0.0f, Speed = 0.1f)
	float EmitterDuration = 5.0f;

	UPROPERTY(DisplayName = "Emitter Loops")
	bool bEmitterLoops = true;

	UPROPERTY(DisplayName = "Max Emitter Loops", Min = 0.0f, Speed = 1.0f)
	int32 MaxEmitterLoops = 0;

	UPROPERTY(DisplayName = "Use Seeded Random")
	bool bUseSeededRandom = false;

	UPROPERTY(DisplayName = "Random Seed")
	int32 RandomSeed = 1;

	UPROPERTY(DisplayName = "Reset Seed On Emitter Loop")
	bool bResetSeedOnEmitterLoop = true;

	UPROPERTY(DisplayName = "Use Fixed Bounds")
	bool bUseFixedBounds = false;

	UPROPERTY(DisplayName = "Fixed Bounds Min")
	FVector FixedBoundsMin = FVector(-100.0f, -100.0f, -100.0f);

	UPROPERTY(DisplayName = "Fixed Bounds Max")
	FVector FixedBoundsMax = FVector(100.0f, 100.0f, 100.0f);

	UPROPERTY(DisplayName = "Bounds Scale", Min = 0.0f, Speed = 0.1f)
	float BoundsScale = 1.0f;

	UPROPERTY(DisplayName = "Material", ReferenceType = Asset)
	UMaterialInterface* Material = nullptr;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSpawn, UParticleModule)

	bool IsSpawnRateModule() const override;

	UPROPERTY(DisplayName = "Spawn Rate", Min = 0.0f, Speed = 1.0f)
	float SpawnRate = 10.0f;

	UPROPERTY(DisplayName = "Rate Scale", Min = 0.0f, Speed = 0.1f)
	float RateScale = 1.0f;

	UPROPERTY(DisplayName = "Process Spawn Rate")
	bool bProcessSpawnRate = true;

	/**
	 * @brief 특정 순간에 particle을 한꺼번에 여러 개 터뜨리듯 생성하는 모드
	 */
	UPROPERTY(DisplayName = "Process Burst")
	bool bProcessBurst = false;

	UPROPERTY(DisplayName = "Burst Count", Min = 0.0f, Speed = 1.0f)
	int32 BurstCount = 0;

	UPROPERTY(DisplayName = "Burst Time", Min = 0.0f, Speed = 0.1f)
	float BurstTime = 0.0f;
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLifetime, UParticleModule)

	UParticleModuleLifetime();

	bool IsSpawnModule() const override;

	UPROPERTY(DisplayName = "Lifetime")
	FParticleFloatDistribution Lifetime;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLocation, UParticleModule)

	bool IsSpawnModule() const override;

	UPROPERTY(DisplayName = "Start Location")
	FParticleVectorDistribution StartLocation;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleVelocity, UParticleModule)

	bool IsSpawnModule() const override;

	UPROPERTY(DisplayName = "Start Velocity")
	FParticleVectorDistribution StartVelocity;
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleColor, UParticleModule)

	UParticleModuleColor();

	bool IsSpawnModule() const override;

	UPROPERTY(DisplayName = "Start Color")
	FParticleColorDistribution StartColor;
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSize, UParticleModule)

	UParticleModuleSize();

	bool IsSpawnModule() const override;

	UPROPERTY(DisplayName = "Start Size")
	FParticleVectorDistribution StartSize;
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleCollision, UParticleModule)

	bool IsUpdateModule() const override;
};

// NOTE: Type-DataBase가 아니라 TypeData-Base 입니다.
//       ㄴ SELECT UParticleModuleTypeDataBase FROM UParticleModule WHERE type = 'sprite'
UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleTypeDataBase, UParticleModule)

	virtual void Build();
	virtual FParticleEmitterInstance* CreateInstance(
		UParticleEmitter* InEmitterTemplate,
		IParticleEmitterInstanceOwner& InOwner);
	virtual FDynamicEmitterDataBase* GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance);
	virtual int32 GetRequiredPayloadSize() const;
};

/**
 * @note: Cascade 스타일을 따라 UParticleModuleTypeDataSprite를 별도로 두지 않습니다.
 *        sprite emitter가 기본 emitter 타입이고, TypeData 모듈은 기본 sprite emitter가 아닌 다른 타입으로 바꿀 때 붙는 모듈입니다.
 */

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataMesh, UParticleModuleTypeDataBase)

	UStaticMesh* Mesh = nullptr;
};

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataRibbon, UParticleModuleTypeDataBase)
};

UCLASS()
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY(UParticleModuleTypeDataBeam, UParticleModuleTypeDataBase)
};
