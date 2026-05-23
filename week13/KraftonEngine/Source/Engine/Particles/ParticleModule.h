#pragma once
#include "Object/Object.h"
#include "Math/Transform.h"
#include "Core/Types/EngineTypes.h"

struct FParticleEmitterInstance;
class FBaseParticle;
class UParticleModuleTypeDataBase;
class UParticleLODLevel;

enum EModuleType : int
{
	/** General - all emitter types can use it			*/
	EPMT_General,
	/** TypeData - TypeData modules						*/
	EPMT_TypeData,
	/** Beam - only applied to beam emitters			*/
	EPMT_Beam,
	/** Trail - only applied to trail emitters			*/
	EPMT_Trail,
	/** Spawn - all emitter types REQUIRE it			*/
	EPMT_Spawn,
	/** Required - all emitter types REQUIRE it			*/
	EPMT_Required,
	/** Event - event related modules					*/
	EPMT_Event,
	/** Light related modules							*/
	EPMT_Light,
	/** SubUV related modules							*/
	EPMT_SubUV,
	EPMT_MAX,
};

struct FParticleCurvePair
{
	FString CurveName;
	UObject* CurveObject;

	FParticleCurvePair()
		: CurveObject(NULL)
	{
	}
};

#include "Source/Engine/Particles/ParticleModule.generated.h"

UCLASS()
class UParticleModule : public UObject
{
public:
	GENERATED_BODY()

	// uint8 이지만 실제로는 1 bit 만 사용하도록 유도
	uint8 bEnabled : 1;
	uint8 bSpawnModule : 1;
	uint8 bUpdateModule : 1;
	uint8 bFinalUpdateModule : 1;

	struct FContext
	{
		FParticleEmitterInstance& Owner;
		const FTransform& GetTransform() const;
		UObject* GetDistributionData() const;
		FString GetTemplateName() const;
		FString GetInstanceName() const;
		FContext(FParticleEmitterInstance& Ow) : Owner(Ow) {}
	};

	struct FSpawnContext : FContext
	{
		int32 Offset;
		float SpawnTime;
		FBaseParticle* ParticleBase;  // 배치 처리를 위한 시작 주소
		/**
		 *	Called on a particle that is freshly spawned by the emitter.
		 *
		 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
		 *	@param	Offset		The modules offset into the data payload of the particle.
		 *	@param	SpawnTime	The time of the spawn.
		 */
		FSpawnContext(FParticleEmitterInstance& Ow, int32 Of, float St, FBaseParticle* Pb) : FContext(Ow), Offset(Of), SpawnTime(St), ParticleBase(Pb) {}
	};
	
	virtual void Spawn(const FSpawnContext& Context);

	struct FUpdateContext : FContext
	{
		int32 Offset;
		float DeltaTime;
		/**
		 *	Called on a particle that is being updated by its emitter.
		 *
		 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
		 *	@param	Offset		The modules offset into the data payload of the particle.
		 *	@param	DeltaTime	The time since the last update.
		 */
		FUpdateContext(FParticleEmitterInstance& Ow, int32 Of, float Dt) : FContext(Ow), Offset(Of), DeltaTime(Dt) {}
	};

	virtual void Update(const FUpdateContext& Context);
	/**
	 *	Called on an emitter when all other update operations have taken place
	 *	INCLUDING bounding box cacluations!
	 */
	virtual void	FinalUpdate(const FUpdateContext& Context);
	// Returns the number of bytes that the module requires in the particle payload block. (particle 별 필요 바이트 라고 함)
	virtual uint32	RequiredBytes(UParticleModuleTypeDataBase* TypeData);
	// Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	virtual uint32	RequiredBytesPerInstance();


	/* 에디터 관련 인터페이스 */
	virtual void GetCurveObjects(TArray<FParticleCurvePair>& OutCurves);

	// curve 변경 후 캐시 리빌드 등
	virtual void RefreshModule();

	/*
	struct FPreviewContext : FContext
	{
		const FSceneView* View;
		FPrimitiveDrawInterface* PDI;
		FPreviewContext(FParticleEmitterInstance& Ow, const FSceneView* Vi, FPrimitiveDrawInterface* Pd) : FContext(Ow), View(Vi), PDI(Pd) {}
	};
	virtual void Render3DPreview(const FPreviewContext& Context);
	*/

	virtual EModuleType GetModuleType() const;

#if WITH_EDITOR
	FColor ModuleEditorColor = FColor::White();
	bool bCurvesAsColor = false;
	bool b3DDrawMode = false;
	bool bSupported3DDrawMode = false;

	virtual void PostEditChangeProperty(const FPropertyChangedEvent& Event) override;

	// Beam Module 인데 Sprite Emitter 이거나 Required Module 누락 등 검증용 함수
	/** Returns true if the module is valid for the provided LOD level. */
	virtual bool IsValidForLODLevel(UParticleLODLevel* LODLevel, FString& OutErrorString);
#endif
};
