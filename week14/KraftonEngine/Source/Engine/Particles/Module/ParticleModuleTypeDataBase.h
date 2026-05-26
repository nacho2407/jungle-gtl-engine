#pragma once

#include "ParticleModule.h"
#include "Component/Particle/ParticleSystemComponent.h"
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

UENUM()
enum class EBeamMethod : uint8
{
        Distance,
        Target,
};

UENUM()
enum class EBeamEndpointMethod : uint8
{
        Default,
        UserSet,
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
	float InitialWidth = 1.0f;
	float EndWidth = 0.0f;
	float Width = 1.0f;
	float Twist = 0.0f;
	uint16 RibbonId = 0;
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
	float Width = 8.0f;
	FVector NoiseRange = FVector::ZeroVector;
	float NoiseFrequency = 0.0f;
	float NoisePhase = 0.0f;
	uint16 BeamIndex = 0;
	bool bAllowTargetModule = true;
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
		Payload->InitialMeshScale = StartMeshScale.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartMeshScale"));
		Payload->MeshScale = Payload->InitialMeshScale;
		Payload->MeshRotation = StartMeshRotation.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "StartMeshRotation"));
		Payload->MeshRotationRate = MeshRotationRate.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "MeshRotationRate"));
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
	UParticleModuleTypeDataRibbon()
	{
		TessellationFactor = 1.0f;
		bUseTrailSmoothing = true;
		TextureTileDistance = 100.0f;

		StartWidth.Constant = 16.0f;
		StartWidth.MinValue = 16.0f;
		StartWidth.MaxValue = 16.0f;

		EndWidth.Constant = 0.0f;
		EndWidth.MinValue = 0.0f;
		EndWidth.MaxValue = 0.0f;

		StartTwist.Constant = 0.0f;
		StartTwist.MinValue = 0.0f;
		StartTwist.MaxValue = 0.0f;
	}

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Ribbon", DisplayName="Tessellation Factor", Min=0.0f, Speed=0.1f)
	float TessellationFactor = 1.0f;
	UPROPERTY(Edit, Save, Category="Particle|TypeData|Ribbon", DisplayName="Use Trail Smoothing")
	bool bUseTrailSmoothing = true;

	UPROPERTY(Edit, Save, Category = "Particle|TypeData|Ribbon", DisplayName = "Texture Tile Distance", Min = 1.0f, Speed = 1.0f)
	float TextureTileDistance = 100.0f;

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Ribbon", DisplayName="Start Width", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat StartWidth;

	UPROPERTY(Edit, Save, Category = "Particle|TypeData|Ribbon", DisplayName = "End Width", Type = Struct, Struct = FRawDistributionFloat)
	FRawDistributionFloat EndWidth;

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Ribbon", DisplayName="Start Twist", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat StartTwist;

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* Emitter, UParticleSystemComponent* Component) override
	{
		return new FParticleRibbonEmitterInstance(Emitter, Component, this);
	}

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FRibbonParticlePayload)) > Owner->ParticleStride)
		{
			return;
		}

		FRibbonParticlePayload* Payload = reinterpret_cast<FRibbonParticlePayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
		Payload->SourcePosition = Particle.Position;
		Payload->PreviousPosition = Particle.Position;
		Payload->InitialWidth = StartWidth.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "StartWidth"));
		Payload->EndWidth = EndWidth.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "EndWidth"));
		Payload->Width = Payload->InitialWidth;
		Payload->Twist = StartTwist.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "StartTwist"));
		Payload->RibbonId = 0;
		Payload->SourceParticleIndex = static_cast<uint16>(Particle.FrameIndex & 0xffff);
		Payload->NextParticleIndex = 0xffff;
	}

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FRibbonParticlePayload)) > Owner->ParticleStride)
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
			FRibbonParticlePayload* Payload = reinterpret_cast<FRibbonParticlePayload*>(ParticleBase + CurrentOffset);
			Payload->PreviousPosition = Payload->SourcePosition;
			Payload->SourcePosition = Particle->Position;

			const float T = Particle->RelativeTime;
			Payload->Width = Payload->InitialWidth * (1.0f - T) + Payload->EndWidth * T;
		END_UPDATE_LOOP
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
	UParticleModuleTypeDataBeam()
	{
		BeamWidth.Constant = 8.0f;
		BeamWidth.MinValue = 8.0f;
		BeamWidth.MaxValue = 8.0f;

		Distance.Constant = 100.0f;
		Distance.MinValue = 100.0f;
		Distance.MaxValue = 100.0f;

		TextureTileDistance = 100.0f;
	}

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Beam", DisplayName="Max Beam Count", Min=1.0f, Speed=1.0f)
	int32 MaxBeamCount = 1;
	UPROPERTY(Edit, Save, Category="Particle|TypeData|Beam", DisplayName="Interpolation Points", Min=0.0f, Speed=1.0f)
	int32 InterpolationPoints = 10;

	UPROPERTY(Edit, Save, Category="Particle|TypeData|Beam", DisplayName="Beam Method", Enum=EBeamMethod)
	EBeamMethod BeamMethod = EBeamMethod::Target;

	UPROPERTY(Edit, Save, Category = "Particle|TypeData|Beam", DisplayName = "Beam Width", Type = Struct, Struct = FRawDistributionFloat)
	FRawDistributionFloat BeamWidth;

	UPROPERTY(Edit, Save, Category = "Particle|TypeData|Beam", DisplayName = "Distance", Type = Struct, Struct = FRawDistributionFloat)
	FRawDistributionFloat Distance;

	UPROPERTY(Edit, Save, Category = "Particle|TypeData|Beam", DisplayName = "Texture Tile Distance", Min = 1.0f, Speed = 1.0f)
	float TextureTileDistance = 100.0f;

	bool IsSpawnModule() const override { return true; }

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* Emitter, UParticleSystemComponent* Component) override
	{
		return new FParticleBeamEmitterInstance(Emitter, Component, this);
	}

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
		{
			return;
		}

		FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
		Payload->SourcePoint = Particle.Position;
		Payload->TargetPoint = Particle.Position + FVector(100.0f, 0.0f, 0.0f);
		Payload->SourceTangent = FVector::ZeroVector;
		Payload->TargetTangent = FVector::ZeroVector;
		Payload->SourceStrength = 0.0f;
		Payload->TargetStrength = 0.0f;

		if (BeamMethod == EBeamMethod::Distance)
		{
			const float EvaluatedDistance = Distance.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "BeamDistance"));
			const FVector Direction = Particle.Velocity.LengthSquared() > 0.0001f ? Particle.Velocity.Normalized() : FVector(1.0f, 0.0f, 0.0f);
			Payload->TargetPoint = Payload->SourcePoint + Direction * EvaluatedDistance;
			Payload->bAllowTargetModule = false;
		}
		else
		{
			Payload->bAllowTargetModule = true;
		}

		Payload->BeamDistance = FVector::Distance(Payload->SourcePoint, Payload->TargetPoint);
		Payload->BeamIndex = static_cast<uint16>(MaxBeamCount > 0 ? Particle.FrameIndex % static_cast<uint32>(MaxBeamCount) : 0);
		Payload->Width = BeamWidth.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "BeamWidth"));
		Payload->NoiseRange = FVector::ZeroVector;
		Payload->NoiseFrequency = 0.0f;
		Payload->NoisePhase = 0.0f;
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

UCLASS()
class UParticleModuleBeamSource : public UParticleModule
{
public:
        GENERATED_BODY()
        UParticleModuleBeamSource()
        {
                SourcePoint.Constant = FVector::ZeroVector;
                SourcePoint.MinValue = FVector::ZeroVector;
                SourcePoint.MaxValue = FVector::ZeroVector;
                SourcePointParameterName = FName("BeamSource");

                SourceTangent.Constant = FVector::ZeroVector;
                SourceTangent.MinValue = FVector::ZeroVector;
                SourceTangent.MaxValue = FVector::ZeroVector;
        }

        bool IsSpawnModule() const override { return true; }
        bool IsUpdateModule() const override { return true; }

        UPROPERTY(Edit, Save, Category="Particle|Beam|Source", DisplayName="Source Method", Enum=EBeamEndpointMethod)
        EBeamEndpointMethod SourceMethod = EBeamEndpointMethod::Default;

        UPROPERTY(Edit, Save, Category="Particle|Beam|Source", DisplayName="Source Parameter", EditCondition="SourceMethod == UserSet")
        FName SourcePointParameterName = FName("BeamSource");

        UPROPERTY(Edit, Save, Category="Particle|Beam|Source", DisplayName="Source Point", EditCondition="SourceMethod == UserSet", Type=Struct, Struct=FRawDistributionVector)
        FRawDistributionVector SourcePoint;

        UPROPERTY(Edit, Save, Category="Particle|Beam|Source", DisplayName="Source Tangent", Type=Struct, Struct=FRawDistributionVector)
        FRawDistributionVector SourceTangent;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
		{
			return;
		}

                FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
                ApplySourcePoint(Owner, *Payload, Particle, SpawnTime);

                Payload->SourceTangent = SourceTangent.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "BeamSourceTangent"));
                Payload->SourceStrength = 1.0f;
                Payload->BeamDistance = FVector::Distance(Payload->SourcePoint, Payload->TargetPoint);
        }

        void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
        {
                if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
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
                        FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(ParticleBase + CurrentOffset);
                        ApplySourcePoint(&Context.Owner, *Payload, *Particle, Particle->RelativeTime);
                        Payload->SourceTangent = SourceTangent.GetValue(Particle->RelativeTime, FDistributionSampling::RandomUnitVector(Particle->RandomSeed, "BeamSourceTangent"));
                        Payload->BeamDistance = FVector::Distance(Payload->SourcePoint, Payload->TargetPoint);
                END_UPDATE_LOOP
        }

        void ApplySourcePoint(FParticleEmitterInstance* Owner, FBeamParticlePayload& Payload, const FBaseParticle& Particle, float Time)
        {
                if (SourceMethod != EBeamEndpointMethod::UserSet)
                {
                        return;
                }

                FVector ParameterValue;
                UParticleSystemComponent* Component = Owner ? Owner->GetComponent() : nullptr;
                if (Component && Component->GetVectorParameter(SourcePointParameterName, ParameterValue))
                {
                        Payload.SourcePoint = ParameterValue;
                        return;
                }

                Payload.SourcePoint = Particle.Position + SourcePoint.GetValue(Time, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "BeamSourcePoint"));
        }
};

UCLASS()
class UParticleModuleBeamNoise : public UParticleModule
{
public:
	GENERATED_BODY()
	UParticleModuleBeamNoise()
	{
		NoiseRange.Constant = FVector::ZeroVector;
		NoiseRange.MinValue = FVector::ZeroVector;
		NoiseRange.MaxValue = FVector::ZeroVector;

		NoiseFrequency.Constant = 4.0f;
		NoiseFrequency.MinValue = 4.0f;
		NoiseFrequency.MaxValue = 4.0f;

		NoiseSpeed.Constant = 0.0f;
		NoiseSpeed.MinValue = 0.0f;
		NoiseSpeed.MaxValue = 0.0f;
	}

	bool IsSpawnModule() const override { return true; }
	bool IsUpdateModule() const override { return true; }

	UPROPERTY(Edit, Save, Category="Particle|Beam|Noise", DisplayName="Noise Range", Type=Struct, Struct=FRawDistributionVector)
	FRawDistributionVector NoiseRange;

	UPROPERTY(Edit, Save, Category="Particle|Beam|Noise", DisplayName="Noise Frequency", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat NoiseFrequency;

	UPROPERTY(Edit, Save, Category="Particle|Beam|Noise", DisplayName="Noise Speed", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat NoiseSpeed;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
		{
			return;
		}

		FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
		Payload->NoiseRange = NoiseRange.GetValue(SpawnTime, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "BeamNoiseRange"));
		Payload->NoiseFrequency = NoiseFrequency.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "BeamNoiseFrequency"));
		Payload->NoisePhase = FDistributionSampling::RandomUnit(Particle.RandomSeed, "BeamNoisePhase");
	}

	void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
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
			FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(ParticleBase + CurrentOffset);
			Payload->NoiseRange = NoiseRange.GetValue(Particle->RelativeTime, FDistributionSampling::RandomUnitVector(Particle->RandomSeed, "BeamNoiseRange"));
			Payload->NoiseFrequency = NoiseFrequency.GetValue(Particle->RelativeTime, FDistributionSampling::RandomUnit(Particle->RandomSeed, "BeamNoiseFrequency"));
			const float Speed = NoiseSpeed.GetValue(Particle->RelativeTime, FDistributionSampling::RandomUnit(Particle->RandomSeed, "BeamNoiseSpeed"));
			Payload->NoisePhase += Speed * DeltaTime;
		END_UPDATE_LOOP
	}
};

UCLASS()
class UParticleModuleBeamTarget : public UParticleModule
{
public:
        GENERATED_BODY()
        UParticleModuleBeamTarget()
        {
                TargetPoint.Constant = FVector(100.0f, 0.0f, 0.0f);
                TargetPoint.MinValue = TargetPoint.Constant;
                TargetPoint.MaxValue = TargetPoint.Constant;
                TargetPointParameterName = FName("BeamEnd");
        }

        bool IsSpawnModule() const override { return true; }
        bool IsUpdateModule() const override { return true; }

        UPROPERTY(Edit, Save, Category="Particle|Beam|Target", DisplayName="Target Method", Enum=EBeamEndpointMethod)
        EBeamEndpointMethod TargetMethod = EBeamEndpointMethod::Default;

        UPROPERTY(Edit, Save, Category="Particle|Beam|Target", DisplayName="Target Parameter", EditCondition="TargetMethod == UserSet")
        FName TargetPointParameterName = FName("BeamEnd");

        UPROPERTY(Edit, Save, Category="Particle|Beam|Target", DisplayName="Target Point", EditCondition="TargetMethod == UserSet", Type=Struct, Struct=FRawDistributionVector)
        FRawDistributionVector TargetPoint;

        UPROPERTY(Edit, Save, Category="Particle|Beam|Target", DisplayName="Target Strength", Type=Struct, Struct=FRawDistributionFloat)
	FRawDistributionFloat TargetStrength;

	void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle& Particle) override
	{
		if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
		{
			return;
		}

		FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
		if (!Payload->bAllowTargetModule)
		{
                        return;
                }

                ApplyTargetPoint(Owner, *Payload, Particle, SpawnTime);
                Payload->TargetStrength = TargetStrength.GetValue(SpawnTime, FDistributionSampling::RandomUnit(Particle.RandomSeed, "BeamTargetStrength"));
                Payload->BeamDistance = FVector::Distance(Payload->SourcePoint, Payload->TargetPoint);
        }

        void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override
        {
                if (!Owner || Offset < 0 || Offset + static_cast<int32>(sizeof(FBeamParticlePayload)) > Owner->ParticleStride)
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
                        FBeamParticlePayload* Payload = reinterpret_cast<FBeamParticlePayload*>(ParticleBase + CurrentOffset);
                        if (!Payload->bAllowTargetModule)
                        {
                                continue;
                        }

                        ApplyTargetPoint(&Context.Owner, *Payload, *Particle, Particle->RelativeTime);
                        Payload->TargetStrength = TargetStrength.GetValue(Particle->RelativeTime, FDistributionSampling::RandomUnit(Particle->RandomSeed, "BeamTargetStrength"));
                        Payload->BeamDistance = FVector::Distance(Payload->SourcePoint, Payload->TargetPoint);
                END_UPDATE_LOOP
        }

        void ApplyTargetPoint(FParticleEmitterInstance* Owner, FBeamParticlePayload& Payload, const FBaseParticle& Particle, float Time)
        {
                if (TargetMethod != EBeamEndpointMethod::UserSet)
                {
                        return;
                }

                FVector ParameterValue;
                UParticleSystemComponent* Component = Owner ? Owner->GetComponent() : nullptr;
                if (Component && Component->GetVectorParameter(TargetPointParameterName, ParameterValue))
                {
                        Payload.TargetPoint = ParameterValue;
                        return;
                }

                Payload.TargetPoint = Payload.SourcePoint + TargetPoint.GetValue(Time, FDistributionSampling::RandomUnitVector(Particle.RandomSeed, "BeamTargetPoint"));
        }
};
