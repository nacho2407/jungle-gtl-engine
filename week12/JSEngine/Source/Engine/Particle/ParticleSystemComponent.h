#pragma once

#include "Component/PrimitiveComponent.h"
#include "Object/ObjectPtr.h"
#include "Particle/ParticleAsset.h"

class UWorld;
class UParticleSystem;
class FParticleEmitterInstance;

struct FDynamicEmitterDataBase;

/******************************************************************
* Particle System 런타임 객체 관리 컴포넌트
* Particle System이 Asset이라면 PSC는 Asset을 참조하는 개별 Instance
*******************************************************************/
UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent();
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	void SetTemplateAsset(const TSoftObjectPtr<UParticleSystem>& InTemplate);
	void SetTemplatePath(const FString& InPath);

	UParticleSystem* GetTemplate();
	const UParticleSystem* GetTemplate() const;

	UWorld* GetWorld() const;

	void Serialize(FArchive& Ar) override;
	void PostEditProperty(const char* PropertyName) override;

	void TickComponent(float DeltaTime) override;
	void PackRenderData();
	int32 GetEmitterRenderDataSnapshotCount() const { return static_cast<int32>(EmitterRenderData.size()); }
	const FDynamicEmitterDataBase* GetEmitterRenderDataSnapshot(int32 SnapshotIndex) const;
	const FParticleFrameStats& GetLastParticleFrameStats() const { return LastParticleFrameStats; }

	/**
	 * @brief 내부 receiver 입력 event 저장
	 */
	void ReportParticleEvent(const FParticleEventPayload& Event);

	/**
	 * @brief particle 이동 구간을 world Shape query로 검사
	 * @param CollisionShape line 또는 이동 sphere query 형상
	 */
	bool ParticleLineCheck(
		FHitResult& Hit,
		AActor* SourceActor,
		const FVector& EndWS,
		const FVector& StartWS,
		const FCollisionShape& CollisionShape);

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
	bool SupportsOutline() const override { return false; }

	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;

	void ResetParticles();

private:
	void CreateEmitterInstances();
	void ReleaseEmitterInstances();
	void ReleaseRenderData();
	void ProcessParticleEventReceivers(bool bHasSoloEmitter);
	void UpdateLastParticleFrameStats();

	/**
	 * @brief 현재 거리와 hysteresis 정책으로 사용할 LOD index를 선택합니다.
	 *
	 * @param EmitterTemplate LOD 후보 개수를 제공하는 emitter template
	 *
	 * @param CurrentLODIndex 현재 emitter instance가 사용 중인 LOD index
	 *
	 * @return 선택된 LOD index
	 *
	 * @details ParticleSystem의 LODDistances와 LODHysteresisDistance를 함께 사용하여 경계 근처의 잦은 LOD 왕복 전환을 방지합니다.
	 */
	int32 SelectLODLevelIndex(const UParticleEmitter* EmitterTemplate, int32 CurrentLODIndex) const;
	void UpdateLODLevel();

	/**
	 * @brief 지정된 emitter template과 LOD index에 맞는 새 emitter instance를 생성합니다.
	 *
	 * @param EmitterTemplate instance를 만들 particle emitter template
	 *
	 * @param LODIndex 생성할 instance가 사용할 LOD index
	 *
	 * @return 생성과 초기화에 성공한 emitter instance. 실패하면 nullptr 반환
	 *
	 * @details TypeDataModule이 있으면 TypeDataModule의 factory를 사용하고, 없으면 기본 FParticleEmitterInstance로 fallback합니다.
	 */
	FParticleEmitterInstance* CreateEmitterInstanceForLOD(UParticleEmitter* EmitterTemplate, int32 LODIndex);

	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_ParticleSystem;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;
	FParticleFrameStats LastParticleFrameStats;

	TArray<FParticleEventPayload> ParticleEvents;
	UParticleSystem* ResolvedTemplate = nullptr;

	// CPP참고 -  EmitterInstance에게 넘겨주는 Component 정보
	class FInstanceOwner;
	std::unique_ptr<FInstanceOwner> InstanceOwner;

	UPROPERTY(DisplayName = "Template", ReferenceType = Asset)
	TSoftObjectPtr<UParticleSystem> Template;
};
