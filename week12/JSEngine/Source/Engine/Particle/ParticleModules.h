#pragma once

#include "Object/Object.h"
#include "Particle/ParticleTypes.h"
#include "Render/Resource/Material.h"

class FParticleEmitterInstance;
class IParticleEmitterInstanceOwner;
class UParticleEmitter;
class UStaticMesh;

UCLASS(Abstract)
class UParticleModule : public UObject
{
public:
	GENERATED_BODY(UParticleModule, UObject)

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) = 0;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) = 0;
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleRequired, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	EParticleSortMode SortMode = EParticleSortMode::ViewDepthBackToFront;
	EParticleCoordinateSpace CoordinateSpace = EParticleCoordinateSpace::Local;
	int32 MaxParticles = 1000;
	float EmitterDuration = 5.0f;
	bool bEmitterLoops = true;
	FMaterial Material;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSpawn, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLifetime, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLocation, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleVelocity, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleColor, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSize, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleCollision, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
};

// NOTE: Type-DataBase가 아니라 TypeData-Base 입니다.
UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleTypeDataBase, UParticleModule)

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	virtual void Build();
	virtual FParticleEmitterInstance* CreateInstance(
		UParticleEmitter* InEmitterTemplate,
		IParticleEmitterInstanceOwner& InOwner);
	virtual FDynamicEmitterDataBase* GetDynamicRenderData(FParticleEmitterInstance* InEmitterInstance);
	virtual int32 GetRequiredPayloadSize() const;
};

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
