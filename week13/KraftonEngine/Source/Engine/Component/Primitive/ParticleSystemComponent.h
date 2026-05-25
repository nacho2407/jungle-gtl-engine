#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Core/Delegate.h"
#include "Source/Engine/Component/Primitive/ParticleSystemComponent.generated.h"

class UParticleSystem;
class UMaterial;
struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

/**
 *	The base class for all particle event data.
 */
struct FParticleEventData
{
	/** The type of event that was generated. */
	int32 Type;

	/** The name of the event. */
	FName EventName;

	/** The emitter time at the event. */
	float EmitterTime;

	/** The location of the event. */
	FVector Location;

	/** The velocity at the time of the event. */
	FVector Velocity;

	FParticleEventData()
		: Type(0)
		, EmitterTime(0)
	{
	}
};

/**
 *	Particle event data for particles that already existed at the time of the event
 */
struct FParticleExistingData : public FParticleEventData
{
	/** How long the particle had been alive at the time of the event. */
	float ParticleTime;

	/** The direction of the particle at the time of the event. */
	FVector Direction;

	FParticleExistingData()
		: ParticleTime(0)
	{
	}
};

/**
 *	Collision particle event data.
 */
struct FParticleEventCollideData : public FParticleExistingData
{
	/** Normal vector in coordinate system of the returner. Zero=none. */
	FVector Normal;

	/** Time until hit, if line check. */
	float Time;

	/** Primitive data item which was hit, INDEX_NONE=none. */
	int32 Item;

	/** Name of bone we hit (for skeletal meshes). */
	FName BoneName;

	/** The physical material for this collision. */
	// UPhysicalMaterial* PhysMat;

	FParticleEventCollideData()
		: Time(0)
		, Item(0)
	{
	}
};

DECLARE_MULTICAST_DELEGATE_OneParam(FParticleCollisionSignature, const FParticleEventCollideData&);

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
	float GetWorldTimeSeconds() const { return CachedWorldTimeSeconds; }

	FParticleCollisionSignature OnParticleCollide;
	TArray<FParticleEventCollideData> CollisionEvents;

	void DispatchCollisionEvents()
	{
		for (const FParticleEventCollideData& E : CollisionEvents)
		{
			OnParticleCollide.Broadcast(E);
		}

		CollisionEvents.clear();
	}

private:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
    void ClearEmitterInstances();
    void ClearRenderData();
    void BuildEmitterInstances();
    void BuildDynamicData();
    void ResolveEmitterMaterialsFromSlots();

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
	float CachedWorldTimeSeconds = 0.0f;
};
