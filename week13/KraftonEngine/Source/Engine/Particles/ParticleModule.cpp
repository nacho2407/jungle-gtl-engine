#include "ParticleModule.h"

const FTransform& UParticleModule::FContext::GetTransform() const
{
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
