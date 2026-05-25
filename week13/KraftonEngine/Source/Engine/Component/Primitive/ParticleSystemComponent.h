#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Source/Engine/Component/Primitive/ParticleSystemComponent.generated.h"

class UParticleSystem;
class UMaterial;
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

	bool IsGameWorld() const { return true; }

    void       SetMaterial(int32 ElementIndex, UMaterial* InMaterial);
    UMaterial* GetMaterial(int32 ElementIndex) const;
    const TArray<UMaterial*>& GetEmitterMaterials() const { return EmitterMaterials; }

    FPrimitiveSceneProxy* CreateSceneProxy() override;
    void                  UpdateWorldAABB() const override;
    void                  PostEditProperty(const char* PropertyName) override;
    void                  PostDuplicate() override;

    const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }
    const TArray<FDynamicEmitterDataBase*>&  GetEmitterRenderData() const { return EmitterRenderData; }

	void SetCachedDistanceToCamera(float InDist) { CachedDistanceToCamera = InDist; }

private:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    void ClearEmitterInstances();
    void ClearRenderData();
    void BuildEmitterInstances();
    void BuildDynamicData();
    void ResolveEmitterMaterialsFromSlots();
    void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
    UPROPERTY(Edit, Save, Category="Particle", DisplayName="Template", AssetType="UParticleSystem")
    FSoftObjectPtr TemplatePath = "None";

    UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Emitter Materials", AssetType="Material")
    TArray<FSoftObjectPtr> EmitterMaterialSlots;

    TObjectPtr<UParticleSystem> Template = nullptr;

    TArray<FParticleEmitterInstance*> EmitterInstances;
    TArray<FDynamicEmitterDataBase*>  EmitterRenderData;
    TArray<UMaterial*>                EmitterMaterials;

    bool bInitialized = false;
	
	float CachedDistanceToCamera = 0.0f;
};
