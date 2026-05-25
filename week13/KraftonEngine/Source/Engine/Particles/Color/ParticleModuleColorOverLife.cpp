#include "Object/GarbageCollection.h"
#include "ParticleModuleColorOverLife.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModuleColorOverLife::UParticleModuleColorOverLife()
{
	bSpawnModule = true;
	bUpdateModule = true;
	bClampAlpha = true;
}

void UParticleModuleColorOverLife::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	
	// 초기 색상 설정
	// UE5 기준 PostInitProperties 에서 처리하는데 현재 프로젝트에는 없으므로 임시 처리
	if (!ColorOverLife.Distribution)
	{
		UDistributionVectorConstant* ConstantColor = UObjectManager::Get().CreateObject<UDistributionVectorConstant>(this);
		ConstantColor->Constant = FVector(1, 1, 1);  // default value
		ColorOverLife.Distribution = ConstantColor;
	}

	if (!AlphaOverLife.Distribution)
	{
		UDistributionFloatConstant* ConstantAlpha = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(this);
		ConstantAlpha->Constant = 1.0f;
		AlphaOverLife.Distribution = ConstantAlpha;
	}

	FVector Color = ColorOverLife.GetValue(0.0f);
	float Alpha = AlphaOverLife.GetValue(0.0f);

	Particle.BaseColor.R = Color.R;
	Particle.BaseColor.G = Color.G;
	Particle.BaseColor.B = Color.B;
	Particle.BaseColor.A = Alpha;
	Particle.Color = Particle.BaseColor;
}

void UParticleModuleColorOverLife::Update(const FUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP
	{
		FVector Color = ColorOverLife.GetValue(Particle.RelativeTime);
		float Alpha = AlphaOverLife.GetValue(Particle.RelativeTime);

		Particle.Color.R = Particle.BaseColor.R * Color.R;
		Particle.Color.G = Particle.BaseColor.G * Color.G;
		Particle.Color.B = Particle.BaseColor.B * Color.B;
		Particle.Color.A = Particle.BaseColor.A * Alpha;

		if (bClampAlpha)
		{
			Particle.Color.A = FMath::Clamp(Particle.Color.A, 0.0f, 1.0f);
		}
	}
	END_UPDATE_LOOP
}

void UParticleModuleColorOverLife::AddReferencedObjects(FReferenceCollector& Collector)
{
	UParticleModuleColorBase::AddReferencedObjects(Collector);
	ColorOverLife.AddReferencedObjects(Collector);
	AlphaOverLife.AddReferencedObjects(Collector);
}

void UParticleModuleColorOverLife::Serialize(FArchive& Ar)
{
	UParticleModule::Serialize(Ar);

	int32 Version = 0;
	Ar << Version;

	ColorOverLife.Serialize(Ar);
	AlphaOverLife.Serialize(Ar);

	bool bClamp = bClampAlpha;
	Ar << bClamp;
	if (Ar.IsLoading())
	{
		bClampAlpha = bClamp ? 1 : 0;
	}
}

#if WITH_EDITOR
void UParticleModuleColorOverLife::PostEditChangeProperty(const FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
