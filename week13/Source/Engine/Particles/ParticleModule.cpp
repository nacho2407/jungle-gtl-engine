#include "ParticleModule.h"

#include "Component/Primitive/ParticleSystemComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Serialization/Archive.h"

UParticleModule::UParticleModule()
{
	bEnabled = true;
	bSpawnModule = false;
	bUpdateModule = false;
	bFinalUpdateModule = false;
}

FTransform UParticleModule::FContext::GetTransform() const
{
	if (Owner.Component)
	{
		return FTransform(
			Owner.Component->GetWorldLocation(),
			Owner.Component->GetWorldRotation(),
			Owner.Component->GetWorldScale());
	}
	return FTransform();
}

UObject* UParticleModule::FContext::GetDistributionData() const
{
	return nullptr;
}

FString UParticleModule::FContext::GetTemplateName() const
{
	return FString();
}

FString UParticleModule::FContext::GetInstanceName() const
{
	return FString();
}

void UParticleModule::Spawn(const FSpawnContext& Context)
{
}

void UParticleModule::Update(const FUpdateContext& Context)
{
}

void UParticleModule::FinalUpdate(const FUpdateContext& Context)
{
}

uint32 UParticleModule::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return uint32();
}

uint32 UParticleModule::RequiredBytesPerInstance()
{
	return uint32();
}

void UParticleModule::GetCurveObjects(TArray<FParticleCurvePair>& OutCurves)
{
}

void UParticleModule::RefreshModule()
{
}

EModuleType UParticleModule::GetModuleType() const
{
	return EModuleType();
}

void UParticleModule::Serialize(FArchive& Ar)
{
	// 의도적으로 Super::Serialize 호출 안 함 — UObject가 ObjectName을 쓰는데,
	// 모듈은 ObjectFactory로 재생성될 때 새 이름을 받으므로 디스크에 박힌 옛 이름은
	// 무의미하고 충돌만 일으킨다.

	int32 Version = 0;
	Ar << Version;

	bool bE = bEnabled;
	bool bS = bSpawnModule;
	bool bU = bUpdateModule;
	bool bF = bFinalUpdateModule;
	Ar << bE;
	Ar << bS;
	Ar << bU;
	Ar << bF;
	if (Ar.IsLoading())
	{
		bEnabled           = bE ? 1 : 0;
		bSpawnModule       = bS ? 1 : 0;
		bUpdateModule      = bU ? 1 : 0;
		bFinalUpdateModule = bF ? 1 : 0;
	}
}

#if WITH_EDITOR
void UParticleModule::PostEditChangeProperty(const FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
}

bool UParticleModule::IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString)
{
	return false;
}
#endif
