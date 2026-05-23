#pragma once

#include "ParticleModule.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Math/Distribution.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"

#include "Source/Engine/Particles/Module/ParticleModuleTypeDataBase.generated.h"


class UStaticMesh;
class UParticleEmitter;
class UParticleSystemComponent;
class UParticleModuleTypeDataMesh;
class UParticleModuleTypeDataRibbon;
class UParticleModuleTypeDataBeam;


enum class EParticleRenderType
{
	Sprite,
	Mesh,
	Ribbon,
	Beam,
	GPU,
};

//Payload Structures for different particle types

struct FParticleMeshPayload
{
	FVector InitialMeshScale = FVector(1.0f, 1.0f, 1.0f);
	FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);
	FVector MeshRotation = FVector::ZeroVector;
	FVector MeshRotationRate = FVector::ZeroVector;
};

struct FRibbonParticlePayload
{
	FVector SourcePosition = FVector::ZeroVector;
	FVector PreviousPosition = FVector::ZeroVector;
	float Width = 1.0f;
	float Twist = 0.0f;
	uint16 SourceParticleIndex = 0;
	uint16 NextParticleIndex = 0;
};

struct FBeamParticlePayload
{
	FVector SourcePoint = FVector::ZeroVector;
	FVector TargetPoint = FVector::ZeroVector;
	FVector SourceTangent = FVector::ZeroVector;
	FVector TargetTangent = FVector::ZeroVector;
	float SourceStrength = 0.0f;
	float TargetStrength = 0.0f;
	float BeamDistance = 0.0f;
	uint16 BeamIndex = 0;
};

struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	FParticleMeshEmitterInstance() = default;
	FParticleMeshEmitterInstance(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent, UParticleModuleTypeDataMesh* InTypeDataModule)
		: TypeDataModule(InTypeDataModule)
	{
		InstancePayloadSize = sizeof(FParticleMeshPayload);
		Init(InEmitter, InComponent);
	}

	UParticleModuleTypeDataMesh* TypeDataModule = nullptr;
};

struct FParticleRibbonEmitterInstance : public FParticleEmitterInstance
{
	FParticleRibbonEmitterInstance() = default;
	FParticleRibbonEmitterInstance(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent, UParticleModuleTypeDataRibbon* InTypeDataModule)
		: TypeDataModule(InTypeDataModule)
	{
		InstancePayloadSize = sizeof(FRibbonParticlePayload);
		Init(InEmitter, InComponent);
	}

	UParticleModuleTypeDataRibbon* TypeDataModule = nullptr;
};

struct FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
	FParticleBeamEmitterInstance() = default;
	FParticleBeamEmitterInstance(UParticleEmitter* InEmitter, UParticleSystemComponent* InComponent, UParticleModuleTypeDataBeam* InTypeDataModule)
		: TypeDataModule(InTypeDataModule)
	{
		InstancePayloadSize = sizeof(FBeamParticlePayload);
		Init(InEmitter, InComponent);
	}

	UParticleModuleTypeDataBeam* TypeDataModule = nullptr;
};

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
public:
	GENERATED_BODY()
	virtual ~UParticleModuleTypeDataBase() = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* Emitter, UParticleSystemComponent* Component)
	{
		(void)Emitter;
		(void)Component;
		return nullptr;
	}

	virtual int32 GetParticlePayloadSize() const
	{
		return 0;
	}

	virtual EParticleRenderType GetRenderType() const
	{
		return EParticleRenderType::Sprite;
	}
};

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UParticleModuleTypeDataMesh()
	{
		StartMeshScale.Constant = FVector::OneVector;
		StartMeshScale.MinValue = FVector::OneVector;
		StartMeshScale.MaxValue = FVector::OneVector;
	}

	UStaticMesh* Mesh = nullptr;

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Mesh", DisplayName="Mesh", AssetType="StaticMesh")
	FSoftObjectPtr MeshPath = "None";

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Mesh", DisplayName="Start Mesh Scale", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartMeshScale;

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Mesh", DisplayName="Start Mesh Rotation", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector StartMeshRotation;

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Mesh", DisplayName="Mesh Rotation Rate", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector MeshRotationRate;

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* Emitter, UParticleSystemComponent* Component) override
	{
		return new FParticleMeshEmitterInstance(Emitter, Component, this);
	}

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FParticleMeshPayload)) > Owner->ParticleStride)
		{
			return;
		}

		FParticleMeshPayload* Payload = reinterpret_cast<FParticleMeshPayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
		Payload->InitialMeshScale = StartMeshScale.GetValue();
		Payload->MeshScale = Payload->InitialMeshScale;
		Payload->MeshRotation = StartMeshRotation.GetValue();
		Payload->MeshRotationRate = MeshRotationRate.GetValue();
	}

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FParticleMeshPayload)) > Owner->ParticleStride)
		{
			return;
		}

		struct
		{
			FParticleEmitterInstance& Owner;
			int32 Offset;
			float DeltaTime;
		} Context{ *Owner, Offset, DeltaTime };

		BEGIN_UPDATE_LOOP
			FParticleMeshPayload* Payload = reinterpret_cast<FParticleMeshPayload*>(ParticleBase + CurrentOffset);
			Payload->MeshRotation += Payload->MeshRotationRate * DeltaTime;
		END_UPDATE_LOOP
	}
	
	EParticleRenderType GetRenderType() const override
	{
		return EParticleRenderType::Mesh;
	}

	int32 GetParticlePayloadSize() const override
	{
		return sizeof(FParticleMeshPayload);
	}
};

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UPROPERTY(Edit, Save, Category="Particle|TypeData|Ribbon", DisplayName="Tessellation Factor", Min=0.0f, Speed=0.1f)
	float TessellationFactor = 1.0f;
	UPROPERTY(Edit, Save, Category="Particle|TypeData|Ribbon", DisplayName="Use Trail Smoothing")
	bool bUseTrailSmoothing = true;

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* Emitter, UParticleSystemComponent* Component) override
	{
		return new FParticleRibbonEmitterInstance(Emitter, Component, this);
	}

	EParticleRenderType GetRenderType() const override
	{
		return EParticleRenderType::Ribbon;
	}

	int32 GetParticlePayloadSize() const override
	{
		return sizeof(FRibbonParticlePayload);
	}
};

UCLASS()
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
public:
	GENERATED_BODY()
	UPROPERTY(Edit, Save, Category="Particle|TypeData|Beam", DisplayName="Max Beam Count", Min=1.0f, Speed=1.0f)
	int32 MaxBeamCount = 1;
	UPROPERTY(Edit, Save, Category="Particle|TypeData|Beam", DisplayName="Interpolation Points", Min=0.0f, Speed=1.0f)
	int32 InterpolationPoints = 10;

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* Emitter, UParticleSystemComponent* Component) override
	{
		return new FParticleBeamEmitterInstance(Emitter, Component, this);
	}

	EParticleRenderType GetRenderType() const override
	{
		return EParticleRenderType::Beam;
	}

	int32 GetParticlePayloadSize() const override
	{
		return sizeof(FBeamParticlePayload);
	}
};
