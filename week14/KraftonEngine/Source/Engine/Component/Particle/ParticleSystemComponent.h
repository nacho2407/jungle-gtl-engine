#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Particle/ParticleSystemComponent.generated.h"

struct FDynamicEmitterDataBase;
struct FParticleEmitterInstance;
class UParticleSystem;

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return ParticleSystemTemplate.Get(); }

	void ResetSystem();
	void Activate() override;
	void Deactivate() override;
	void EndPlay() override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void PostDuplicate() override;
	void PostEditProperty(const char* PropertyName) override;

	const TArray<FDynamicEmitterDataBase*>& GetEmitterRenderData() const { return EmitterRenderData; }

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void InitializeEmitterInstances();
	void ClearEmitterInstances();
	void ClearEmitterRenderData();
	void LoadTemplateFromPath();

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TObjectPtr<UParticleSystem> ParticleSystemTemplate;

	UPROPERTY(Edit, Save, Category="Particle", DisplayName="Particle System", AssetType="UParticleSystem")
	FSoftObjectPtr ParticleSystemTemplatePath = "None";

	TArray<FDynamicEmitterDataBase*> EmitterRenderData;
};
