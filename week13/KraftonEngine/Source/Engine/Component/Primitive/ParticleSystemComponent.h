#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Primitive/ParticleSystemComponent.generated.h"

class UParticleSystem;
struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
    GENERATED_BODY()

    UParticleSystemComponent();
    ~UParticleSystemComponent() override;

    void             SetTemplate(UParticleSystem* InTemplate);
    UParticleSystem* GetTemplate() const { return Template.Get(); }

    void InitializeSystem();
    void ResetSystem();

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    void                  UpdateWorldAABB() const override;
    void                  PostEditProperty(const char* PropertyName) override;
    void                  PostDuplicate() override;

    const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }
    const TArray<FDynamicEmitterDataBase*>&  GetEmitterRenderData() const { return EmitterRenderData; }

private:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    void ClearEmitterInstances();
    void ClearRenderData();
    void BuildEmitterInstances();
    void BuildDynamicData();

private:
    UPROPERTY(Edit, Save, Category="Particle", DisplayName="Template", AssetType="UParticleSystem")
    FSoftObjectPtr TemplatePath = "None";

    TObjectPtr<UParticleSystem> Template = nullptr;

    TArray<FParticleEmitterInstance*> EmitterInstances;
    TArray<FDynamicEmitterDataBase*>  EmitterRenderData;

    bool bInitialized = false;
};
